<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
if(current_user()!==null)redirect_to('index.php');
$error='';
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token. Reload and try again.');}
 $username=substr(strtolower(trim((string)($_POST['username']??''))),0,80);$password=(string)($_POST['password']??'');$ip=substr((string)($_SERVER['REMOTE_ADDR']??''),0,45);
 $q=db()->prepare("SELECT COUNT(*) FROM login_attempts WHERE username_key=:u AND ip_address=:ip AND attempted_at>=datetime('now','-15 minutes')");$q->execute(['u'=>$username,'ip'=>$ip]);$blocked=(int)$q->fetchColumn()>=5;
 $q=db()->prepare('SELECT id,username,display_name,password_hash,role,status FROM users WHERE username=:u COLLATE NOCASE LIMIT 1');$q->execute(['u'=>$username]);$user=$q->fetch();
 if(!$blocked&&$user&&$user['status']==='active'&&password_verify($password,(string)$user['password_hash'])){
  session_regenerate_id(true);$_SESSION['user']=['id'=>(int)$user['id'],'username'=>(string)$user['username'],'display_name'=>(string)$user['display_name'],'role'=>(string)$user['role']];
  $q=db()->prepare('DELETE FROM login_attempts WHERE username_key=:u AND ip_address=:ip');$q->execute(['u'=>$username,'ip'=>$ip]);
  $q=db()->prepare("UPDATE users SET last_login_at=datetime('now') WHERE id=:id");$q->execute(['id'=>$user['id']]);
  audit_log((int)$user['id'],'auth.login_success','user',(string)$user['id']);redirect_to('index.php');
 }
 if(!$blocked){$q=db()->prepare("INSERT INTO login_attempts(username_key,ip_address,attempted_at) VALUES(:u,:ip,datetime('now'))");$q->execute(['u'=>$username,'ip'=>$ip]);}
 audit_log($user?(int)$user['id']:null,$blocked?'auth.login_throttled':'auth.login_failed','user',$user?(string)$user['id']:null,['username'=>$username]);
 $error='Sign-in failed. Check your details or try again later.';
}
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Sign in · Staff Sales</title><link rel="stylesheet" href="<?= e(app_url('assets/css/app.css')) ?>"></head>
<body class="auth-page"><main class="auth-shell"><a class="brand" href="<?= e(app_url('login.php')) ?>"><span class="brand-mark">SS</span><span><strong>Staff Sales</strong><small>Internal auction portal</small></span></a>
<section class="auth-panel"><p class="eyebrow">EMPLOYEE ACCESS</p><h1>Welcome back.</h1><p class="muted">Sign in to view staff sale items and your auction activity.</p>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<form method="post" action="<?= e(app_url('login.php')) ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><label for="username">Username</label><input id="username" name="username" required maxlength="80" autocomplete="username" autofocus><label for="password">Password</label><input id="password" name="password" type="password" required autocomplete="current-password"><button class="button button-primary button-wide" type="submit">Sign in →</button></form>
<p class="fine-print">Access is restricted to authorised staff accounts. Contact the portal administrator if you need an account.</p></section><footer>Staff Sales Auction Portal · Foundation v0.1</footer></main></body></html>
