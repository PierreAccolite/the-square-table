<?php
declare(strict_types=1);
/** Apply additive database updates without recreating or resetting existing data. */
$configFile=__DIR__.'/../config/local.php';
if(!is_file($configFile)){fwrite(STDERR,"Missing config/local.php. Configure the project first.\n");exit(1);}
$config=require $configFile;
if(($config['db']['driver']??'sqlite')!=='sqlite'){fwrite(STDERR,"This release supports SQLite only.\n");exit(1);}
$path=(string)($config['db']['path']??'');
if($path===''||!is_file($path)){fwrite(STDERR,"Database file not found: ".$path."\n");exit(1);}
$pdo=new PDO('sqlite:'.$path,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC]);
$pdo->exec('PRAGMA foreign_keys=ON');
$pdo->exec("CREATE TABLE IF NOT EXISTS schema_migrations (version TEXT PRIMARY KEY, applied_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')))");
$columns=$pdo->query('PRAGMA table_info(auction_items)')->fetchAll();$names=array_column($columns,'name');
if(!in_array('buyout_enabled',$names,true))$pdo->exec('ALTER TABLE auction_items ADD COLUMN buyout_enabled INTEGER NOT NULL DEFAULT 0 CHECK(buyout_enabled IN (0,1))');
if(!in_array('buyout_price_cents',$names,true))$pdo->exec('ALTER TABLE auction_items ADD COLUMN buyout_price_cents INTEGER NULL CHECK(buyout_price_cents IS NULL OR buyout_price_cents >= 0)');
$pdo->prepare('INSERT OR IGNORE INTO schema_migrations(version) VALUES(:v)')->execute(['v'=>'002_buyout_settings']);
$pdo->beginTransaction();
try{
 $pdo->exec("CREATE TABLE IF NOT EXISTS user_permissions (user_id INTEGER NOT NULL,permission TEXT NOT NULL CHECK(permission IN ('bidder','procurement','custodian','approver')),granted_by INTEGER,granted_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),PRIMARY KEY(user_id,permission),FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE,FOREIGN KEY(granted_by) REFERENCES users(id) ON DELETE SET NULL)");
 $pdo->exec('CREATE INDEX IF NOT EXISTS idx_user_permissions_permission ON user_permissions(permission,user_id)');
 $pdo->exec("CREATE TABLE IF NOT EXISTS approval_requests (id INTEGER PRIMARY KEY AUTOINCREMENT,request_type TEXT NOT NULL CHECK(request_type IN ('item_change')),entity_type TEXT NOT NULL CHECK(entity_type IN ('auction_item')),entity_id INTEGER NOT NULL,requested_by INTEGER NOT NULL,reason TEXT NOT NULL CHECK(length(reason) BETWEEN 5 AND 2000),before_json TEXT NOT NULL,after_json TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','approved','rejected','stale')),reviewed_by INTEGER,review_comment TEXT,created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),reviewed_at TEXT,FOREIGN KEY(requested_by) REFERENCES users(id) ON DELETE RESTRICT,FOREIGN KEY(reviewed_by) REFERENCES users(id) ON DELETE RESTRICT)");
 $pdo->exec('CREATE INDEX IF NOT EXISTS idx_approval_status_created ON approval_requests(status,created_at)');
 $pdo->exec('CREATE INDEX IF NOT EXISTS idx_approval_entity ON approval_requests(entity_type,entity_id)');
 $pdo->prepare('INSERT OR IGNORE INTO schema_migrations(version) VALUES(:v)')->execute(['v'=>'003_permissions_approvals']);
 $pdo->commit();
}catch(Throwable $e){if($pdo->inTransaction())$pdo->rollBack();throw $e;}
// Migration 004: user email addresses and expiring, single-use password-reset tokens.
$userColumns=$pdo->query('PRAGMA table_info(users)')->fetchAll();
$userColumnNames=array_column($userColumns,'name');
if(!in_array('email',$userColumnNames,true))$pdo->exec('ALTER TABLE users ADD COLUMN email TEXT');
$pdo->exec("CREATE UNIQUE INDEX IF NOT EXISTS idx_users_email_unique ON users(email COLLATE NOCASE) WHERE email IS NOT NULL AND email <> ''");
$pdo->exec("CREATE TABLE IF NOT EXISTS password_reset_tokens (id INTEGER PRIMARY KEY AUTOINCREMENT,user_id INTEGER NOT NULL,token_hash TEXT NOT NULL UNIQUE,expires_at TEXT NOT NULL,used_at TEXT,created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE)");
$pdo->exec('CREATE INDEX IF NOT EXISTS idx_password_reset_user_expiry ON password_reset_tokens(user_id,expires_at,used_at)');
$pdo->prepare('INSERT OR IGNORE INTO schema_migrations(version) VALUES(:v)')->execute(['v'=>'004_password_reset']);
// Migration 005: durable results and retryable winner notification state.
$pdo->exec("CREATE TABLE IF NOT EXISTS auction_results (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 auction_item_id INTEGER NOT NULL UNIQUE,
 auction_id INTEGER NOT NULL,
 result_status TEXT NOT NULL CHECK(result_status IN ('sold','unsold')),
 winning_bid_id INTEGER,
 winner_user_id INTEGER,
 final_price_cents INTEGER NOT NULL DEFAULT 0 CHECK(final_price_cents >= 0),
 finalized_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),
 email_status TEXT NOT NULL DEFAULT 'not_applicable' CHECK(email_status IN ('not_applicable','pending','sent','failed','no_email')),
 email_sent_at TEXT,
 email_last_error TEXT,
 FOREIGN KEY(auction_item_id) REFERENCES auction_items(id) ON DELETE RESTRICT,
 FOREIGN KEY(auction_id) REFERENCES auctions(id) ON DELETE RESTRICT,
 FOREIGN KEY(winning_bid_id) REFERENCES bids(id) ON DELETE RESTRICT,
 FOREIGN KEY(winner_user_id) REFERENCES users(id) ON DELETE RESTRICT
)");
$pdo->exec('CREATE INDEX IF NOT EXISTS idx_auction_results_auction ON auction_results(auction_id,result_status,finalized_at)');
$pdo->exec('CREATE INDEX IF NOT EXISTS idx_auction_results_winner ON auction_results(winner_user_id,finalized_at)');
$pdo->exec('CREATE INDEX IF NOT EXISTS idx_auction_results_email ON auction_results(email_status,finalized_at)');
$pdo->prepare('INSERT OR IGNORE INTO schema_migrations(version) VALUES(:v)')->execute(['v'=>'005_auction_results']);
// Migration 006: categorise lots without changing existing auction data.
$itemColumns=$pdo->query('PRAGMA table_info(auction_items)')->fetchAll();
$itemColumnNames=array_column($itemColumns,'name');
if(!in_array('category',$itemColumnNames,true))$pdo->exec("ALTER TABLE auction_items ADD COLUMN category TEXT NOT NULL DEFAULT 'Uncategorised'");
$pdo->exec('CREATE INDEX IF NOT EXISTS idx_auction_items_category ON auction_items(category,auction_id,status)');
$pdo->exec("CREATE TABLE IF NOT EXISTS item_categories (id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL COLLATE NOCASE UNIQUE CHECK(length(name) BETWEEN 1 AND 80),is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')))");
foreach(['IT','Furniture','Spares','Tools','Office equipment','Vehicles','Other','Uncategorised'] as $categoryName){$seed=$pdo->prepare('INSERT OR IGNORE INTO item_categories(name) VALUES(:name)');$seed->execute(['name'=>$categoryName]);}
$pdo->exec("CREATE TABLE IF NOT EXISTS app_settings (setting_key TEXT PRIMARY KEY,setting_value TEXT NOT NULL,updated_by INTEGER,updated_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),FOREIGN KEY(updated_by) REFERENCES users(id) ON DELETE SET NULL)");
$pdo->prepare('INSERT OR IGNORE INTO schema_migrations(version) VALUES(:v)')->execute(['v'=>'006_item_categories']);
$pdo->prepare('INSERT OR IGNORE INTO schema_migrations(version) VALUES(:v)')->execute(['v'=>'007_mail_settings']);
echo "Migrations 002–006 applied or already present. Existing data preserved.\n";
echo "Permissions, approvals, password reset, item categories, bidding and auction result storage are available.\n";
echo "Configure and test the scheduled CLI finalisation job before operational use.\n";
