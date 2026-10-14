<?php
declare(strict_types=1);
/**
 * Apply the additive 002 buyout-settings migration safely to an existing install.
 * Run from the project root: php tools/apply_migrations.php
 */
$configFile=__DIR__.'/../config/local.php';
if(!is_file($configFile)){fwrite(STDERR,"Missing config/local.php. Configure the project first.\n");exit(1);}
$config=require $configFile;
if(($config['db']['driver']??'sqlite')!=='sqlite'){fwrite(STDERR,"This release supports SQLite only.\n");exit(1);}
$path=(string)($config['db']['path']??'');
if($path===''||!is_file($path)){fwrite(STDERR,"Database file not found: ".$path."\n");exit(1);}
$pdo=new PDO('sqlite:'.$path,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC]);
$pdo->exec('PRAGMA foreign_keys=ON');
$pdo->exec("CREATE TABLE IF NOT EXISTS schema_migrations (version TEXT PRIMARY KEY, applied_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')))");
$columns=$pdo->query('PRAGMA table_info(auction_items)')->fetchAll();
$names=array_column($columns,'name');
if(!in_array('buyout_enabled',$names,true))$pdo->exec('ALTER TABLE auction_items ADD COLUMN buyout_enabled INTEGER NOT NULL DEFAULT 0 CHECK(buyout_enabled IN (0,1))');
if(!in_array('buyout_price_cents',$names,true))$pdo->exec('ALTER TABLE auction_items ADD COLUMN buyout_price_cents INTEGER NULL CHECK(buyout_price_cents IS NULL OR buyout_price_cents >= 0)');
$pdo->prepare('INSERT OR IGNORE INTO schema_migrations(version) VALUES(:v)')->execute(['v'=>'002_buyout_settings']);
echo "Migration 002_buyout_settings applied or already present. Existing item data was preserved.\n";
echo "Note: this stores buyout settings only; purchase processing remains disabled until live bidding is implemented.\n";
