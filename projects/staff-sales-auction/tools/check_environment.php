<?php
declare(strict_types=1);
if(PHP_SAPI!=='cli'){http_response_code(404);exit;}
echo "Staff Sales Auction Portal — environment check\nPHP: ".PHP_VERSION."\n";
foreach(['pdo','pdo_mysql','json','mbstring'] as $x)echo sprintf("%-12s %s\n",$x.':',extension_loaded($x)?'OK':'MISSING');
$f=dirname(__DIR__).'/config/local.php';
if(!is_file($f)){echo "Configuration: MISSING (copy config/config.example.php to config/local.php)\n";exit(1);}
$c=require $f;$d=$c['db']??[];
try{
 $dsn=sprintf('mysql:host=%s;port=%d;dbname=%s;charset=%s',$d['host']??'127.0.0.1',(int)($d['port']??3306),$d['database']??'',$d['charset']??'utf8mb4');
 $pdo=new PDO($dsn,(string)($d['username']??''),(string)($d['password']??''),[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION]);
 echo "Database: CONNECTED\n";$tables=$pdo->query('SHOW TABLES')->fetchAll(PDO::FETCH_COLUMN);echo "Tables found: ".count($tables)."\n";
 if(!in_array('users',$tables,true)||!in_array('auctions',$tables,true)||!in_array('bids',$tables,true)){echo "Schema: INCOMPLETE (apply initial migration)\n";exit(1);}
 echo "Schema: OK\n";
}catch(Throwable $e){echo "Database: FAILED (".$e->getMessage().")\n";exit(1);}
