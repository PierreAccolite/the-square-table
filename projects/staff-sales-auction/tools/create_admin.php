<?php
declare(strict_types=1);
if(PHP_SAPI!=='cli'){fwrite(STDERR,"Run this tool from the command line only.\n");exit(1);}
$configFile=dirname(__DIR__).'/config/local.php';
if(!is_file($configFile)){fwrite(STDERR,"Missing config/local.php. Run python3 tools/setup_local.py first.\n");exit(1);}
$c=require $configFile;$d=$c['db']??[];$path=(string)($d['path']??'');
if(($d['driver']??'')!=='sqlite'||$path===''||!is_file($path)){fwrite(STDERR,"SQLite database not found. Run python3 tools/setup_local.py first.\n");exit(1);}
try{
 $pdo=new PDO('sqlite:'.$path,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC,PDO::ATTR_EMULATE_PREPARES=>false]);$pdo->exec('PRAGMA foreign_keys=ON');
 $tables=$pdo->query("SELECT name FROM sqlite_master WHERE type='table'")->fetchAll(PDO::FETCH_COLUMN);
 if(!in_array('users',$tables,true)||!in_array('audit_events',$tables,true))throw new RuntimeException('Schema missing or incomplete.');
 $username=strtolower(trim(prompt('Administrator username: ')));$name=trim(prompt('Display name: '));$employee=trim(prompt('Employee number (optional): '));
 if(!preg_match('/^[a-z0-9._-]{3,80}$/',$username))throw new RuntimeException('Username must be 3-80 lowercase letters, numbers, dot, underscore or hyphen.');
 if($name===''||mb_strlen($name)>160)throw new RuntimeException('Display name is required (maximum 160 characters).');
 if(mb_strlen($employee)>80)throw new RuntimeException('Employee number must be at most 80 characters.');
 $q=$pdo->prepare('SELECT id FROM users WHERE username=:u COLLATE NOCASE');$q->execute(['u'=>$username]);if($q->fetch())throw new RuntimeException('Username already exists.');
 $password=prompt_secret('Administrator password (12+ characters): ');$confirm=prompt_secret('Confirm password: ');
 if(!hash_equals($password,$confirm))throw new RuntimeException('Passwords do not match.');
 if(strlen($password)<12)throw new RuntimeException('Password must be at least 12 characters.');
 $q=$pdo->prepare("INSERT INTO users(username,display_name,employee_number,password_hash,role,status) VALUES(:u,:n,:e,:p,'admin','active')");
 $q->execute(['u'=>$username,'n'=>$name,'e'=>$employee!==''?$employee:null,'p'=>password_hash($password,PASSWORD_DEFAULT)]);
 $id=(int)$pdo->lastInsertId();$details=json_encode(['role'=>'admin'],JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR);
 $q=$pdo->prepare("INSERT INTO audit_events(actor_user_id,event_type,entity_type,entity_id,details_json) VALUES(:id,'admin.bootstrap_created','user',:entity,:details)");
 $q->execute(['id'=>$id,'entity'=>(string)$id,'details'=>$details]);
 fwrite(STDOUT,"Administrator created (user ID {$id}). The password was not saved to a file.\n");
}catch(Throwable $e){fwrite(STDERR,"Setup failed: ".$e->getMessage()."\n");exit(1);}
function prompt(string $label):string{fwrite(STDOUT,$label);$line=fgets(STDIN);return $line===false?'':rtrim($line,"\r\n");}
function prompt_secret(string $label):string{
 $state=null;
 if(PHP_OS_FAMILY!=='Windows'&&function_exists('shell_exec')){$candidate=shell_exec('stty -g 2>/dev/null');if(is_string($candidate)&&trim($candidate)!==''){$state=trim($candidate);shell_exec('stty -echo 2>/dev/null');}}
 try{$value=prompt($label);}finally{if($state!==null){shell_exec('stty '.escapeshellarg($state).' 2>/dev/null');fwrite(STDOUT,"\n");}}
 return $value;
}
