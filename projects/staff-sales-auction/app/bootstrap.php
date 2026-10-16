<?php
declare(strict_types=1);
$configFile=dirname(__DIR__).'/config/local.php';
if(!is_file($configFile)){http_response_code(503);exit('Run python3 tools/setup_local.py from the project folder to configure the local database.');}
$config=require $configFile;
date_default_timezone_set((string)($config['timezone']??'Africa/Windhoek'));
if(PHP_SAPI!=='cli'&&session_status()!==PHP_SESSION_ACTIVE){
 ini_set('session.use_strict_mode','1');ini_set('session.use_only_cookies','1');ini_set('session.cookie_httponly','1');
 ini_set('session.cookie_secure',!empty($config['session_secure'])?'1':'0');ini_set('session.cookie_samesite','Lax');session_name('staff_sales_session');session_start();
}
function app_config(?string $key=null):mixed{global $config;return $key===null?$config:($config[$key]??null);}
function db():PDO{
 static $pdo=null;if($pdo instanceof PDO)return $pdo;$s=(array)app_config('db');
 if(($s['driver']??'sqlite')!=='sqlite')throw new RuntimeException('This release supports SQLite only.');
 $path=(string)($s['path']??'');if($path==='')throw new RuntimeException('SQLite database path is not configured.');
 $pdo=new PDO('sqlite:'.$path,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION,PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC,PDO::ATTR_EMULATE_PREPARES=>false]);
 $pdo->exec('PRAGMA foreign_keys=ON');$pdo->exec('PRAGMA busy_timeout='.max(1000,(int)($s['busy_timeout_ms']??5000)));return $pdo;
}
function e(mixed $v):string{return htmlspecialchars((string)$v,ENT_QUOTES|ENT_SUBSTITUTE,'UTF-8');}
function app_url(string $p=''):string{return rtrim((string)app_config('base_path'),'/').'/'.ltrim($p,'/');}
function redirect_to(string $p):never{header('Location: '.app_url($p),true,303);exit;}
function csrf_token():string{if(!isset($_SESSION['csrf_token'])||!is_string($_SESSION['csrf_token']))$_SESSION['csrf_token']=bin2hex(random_bytes(32));return $_SESSION['csrf_token'];}
function verify_csrf(?string $t):bool{return is_string($t)&&isset($_SESSION['csrf_token'])&&is_string($_SESSION['csrf_token'])&&hash_equals($_SESSION['csrf_token'],$t);}
function current_user():?array{
 if(!isset($_SESSION['user'])||!is_array($_SESSION['user'])||empty($_SESSION['user']['id']))return null;
 static $checked=false;static $fresh=null;if($checked)return $fresh;$checked=true;
 $q=db()->prepare('SELECT id,username,display_name,role,status FROM users WHERE id=:id LIMIT 1');$q->execute(['id'=>(int)$_SESSION['user']['id']]);$row=$q->fetch();
 if(!$row||$row['status']!=='active'){$_SESSION=[];session_regenerate_id(true);return null;}
 $fresh=['id'=>(int)$row['id'],'username'=>(string)$row['username'],'display_name'=>(string)$row['display_name'],'role'=>(string)$row['role']];$_SESSION['user']=$fresh;return $fresh;
}
function require_login():array{$u=current_user();if($u===null)redirect_to('login.php');return $u;}
function require_role(string ...$roles):array{$u=require_login();if(!in_array((string)($u['role']??''),$roles,true)){http_response_code(403);exit('You do not have permission to access this page.');}return $u;}

function user_permissions(int $userId):array{
 static $cache=[];if(isset($cache[$userId]))return $cache[$userId];
 try{$q=db()->prepare('SELECT permission FROM user_permissions WHERE user_id=:id ORDER BY permission');$q->execute(['id'=>$userId]);return $cache[$userId]=array_values(array_map('strval',$q->fetchAll(PDO::FETCH_COLUMN)));}
 catch(Throwable $ex){return $cache[$userId]=[];}
}
function has_permission(array $user,string $permission):bool{
 if(($user['role']??'')==='admin')return true;
 if($permission==='bidder'&&in_array(($user['role']??''),['employee','admin'],true))return true;
 return in_array($permission,user_permissions((int)($user['id']??0)),true);
}
function require_permission(string ...$permissions):array{
 $u=require_login();foreach($permissions as $permission){if(has_permission($u,$permission))return $u;}
 http_response_code(403);exit('You do not have permission to access this page.');
}
function save_user_permissions(int $userId,array $permissions,int $actorId):void{
 $allowed=['bidder','procurement','custodian','approver'];$permissions=array_values(array_unique(array_intersect($permissions,$allowed)));
 $pdo=db();$pdo->prepare('DELETE FROM user_permissions WHERE user_id=:id')->execute(['id'=>$userId]);
 $q=$pdo->prepare('INSERT INTO user_permissions(user_id,permission,granted_by) VALUES(:id,:permission,:actor)');
 foreach($permissions as $permission)$q->execute(['id'=>$userId,'permission'=>$permission,'actor'=>$actorId]);
}
function audit_log(?int $actor,string $event,?string $type=null,?string $entity=null,array $details=[]):void{
 try{$s=db()->prepare('INSERT INTO audit_events(actor_user_id,event_type,entity_type,entity_id,ip_address,user_agent,details_json) VALUES(:actor,:event,:type,:entity,:ip,:agent,:details)');
 $s->execute(['actor'=>$actor,'event'=>$event,'type'=>$type,'entity'=>$entity,'ip'=>substr((string)($_SERVER['REMOTE_ADDR']??''),0,45)?:null,'agent'=>substr((string)($_SERVER['HTTP_USER_AGENT']??''),0,500)?:null,'details'=>json_encode($details,JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_THROW_ON_ERROR)]);
 }catch(Throwable $ex){error_log('Staff Sales audit write failed: '.$ex->getMessage());if((string)app_config('app_env')==='production')throw $ex;}
}
