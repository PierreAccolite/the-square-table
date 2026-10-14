<?php
declare(strict_types=1);
if(PHP_SAPI!=='cli'){http_response_code(404);exit;}
echo "Staff Sales Auction Portal — environment check\nPHP: ".PHP_VERSION."\n";
foreach(['pdo','pdo_sqlite','json','mbstring','fileinfo'] as $x){printf("%-12s %s\n",$x.':',extension_loaded($x)?'OK':'MISSING');if(!extension_loaded($x))exit(1);}
$f=dirname(__DIR__).'/config/local.php';if(!is_file($f)){echo "Configuration: MISSING (run python3 tools/setup_local.py)\n";exit(1);}
$c=require $f;$d=$c['db']??[];$path=(string)($d['path']??'');
if(($d['driver']??'')!=='sqlite'||$path===''){echo "Database configuration: INVALID (SQLite expected)\n";exit(1);}
try{$pdo=new PDO('sqlite:'.$path,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION]);$pdo->exec('PRAGMA foreign_keys=ON');
 $tables=$pdo->query("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")->fetchAll(PDO::FETCH_COLUMN);
 echo "Database: CONNECTED\nDatabase file: ".$path."\nTables found: ".count($tables)."\n";
 foreach(['users','auctions','auction_items','bids','audit_events','login_attempts','notifications'] as $t){if(!in_array($t,$tables,true)){echo "Schema: INCOMPLETE (missing {$t})\n";exit(1);}}
 echo "Schema: OK\nForeign keys: ".((int)$pdo->query('PRAGMA foreign_keys')->fetchColumn()===1?'ON':'OFF')."\n";
}catch(Throwable $e){echo "Database: FAILED (".$e->getMessage().")\n";exit(1);}
