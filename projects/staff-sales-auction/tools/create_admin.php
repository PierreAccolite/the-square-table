<?php
declare(strict_types=1);
if (PHP_SAPI !== 'cli') { fwrite(STDERR, "Run this tool from the command line only.\n"); exit(1); }
$configFile=dirname(__DIR__).'/config/local.php';
if(!is_file($configFile)){fwrite(STDERR,"Missing config/local.php. Copy config/config.example.php and configure it first.\n");exit(1);}
$c=require $configFile;$d=$c['db']??[];
try{
 $dsn=sprintf('mysql:host=%s;port=%d;dbname=%s;charset=%s',$d['host']??'127.0.0.1',(int)($d['port']??3306),$d['database']??'',$d['charset']??'utf8mb4');
 $pdo=new PDO($dsn,(string)($d['username']??''),(string)($d['password']??''),[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC,PDO::ATTR_EMULATE_PREPARES=>false]);
 if(!$pdo->query("SHOW TABLES LIKE 'users'")->fetchColumn())throw new RuntimeException('Schema missing; apply database/migrations/001_initial_schema.sql first.');
 $username=strtolower(trim((string)readline('Administrator username: ')));
 $name=trim((string)readline('Display name: '));$employee=trim((string)readline('Employee number (optional): '));
 if(!preg_match('/^[a-z0-9._-]{3,80}$/',$username))throw new RuntimeException('Username must be 3-80 lowercase letters, numbers, dot, underscore or hyphen.');
 if($name===''||mb_strlen($name)>160)throw new RuntimeException('Display name is required (maximum 160 characters).');
 if(mb_strlen($employee)>80)throw new RuntimeException('Employee number must be at most 80 characters.');
 $q=$pdo->prepare('SELECT id FROM users WHERE username=:u');$q->execute(['u'=>$username]);if($q->fetch())throw new RuntimeException('Username already exists.');
 fwrite(STDOUT,"Note: password input may be visible in this console. Use a trusted local terminal.\n");
 $password=(string)readline('Password (12+ characters): ');$confirm=(string)readline('Confirm password: ');
 if(!hash_equals($password,$confirm))throw new RuntimeException('Passwords do not match.');
 if(strlen($password)<12)throw new RuntimeException('Password must be at least 12 characters.');
 $q=$pdo->prepare("INSERT INTO users(username,display_name,employee_number,password_hash,role,status) VALUES(:u,:n,:e,:p,'admin','active')");
 $q->execute(['u'=>$username,'n'=>$name,'e'=>$employee!==''?$employee:null,'p'=>password_hash($password,PASSWORD_DEFAULT)]);
 $id=(int)$pdo->lastInsertId();
 $q=$pdo->prepare("INSERT INTO audit_events(actor_user_id,event_type,entity_type,entity_id,details_json) VALUES(:id,'admin.bootstrap_created','user',:entity,JSON_OBJECT('role','admin'))");$q->execute(['id'=>$id,'entity'=>(string)$id]);
 fwrite(STDOUT,"Administrator created (user ID {$id}). Store the password using approved company procedures.\n");
}catch(Throwable $e){fwrite(STDERR,"Setup failed: ".$e->getMessage()."\n");exit(1);}
