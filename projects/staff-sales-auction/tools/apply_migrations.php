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
echo "Migrations 002 and 003 applied or already present. Existing data preserved.\n";
echo "Procurement permissions and item-change approval queue are now available; bidding and buyout transactions remain disabled.\n";
