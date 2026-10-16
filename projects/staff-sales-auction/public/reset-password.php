<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
if(current_user()!==null)redirect_to('index.php');
$token=trim((string)($_GET['token']??$_POST['token']??''));
$validToken=preg_match('/^[a-f0-9]{64}$/',$token)===1;
$error='';$complete=false;$canReset=false;
if($validToken){$q=db()->prepare("SELECT id FROM password_reset_tokens WHERE token_hash=:hash AND used_at IS NULL AND expires_at>datetime('now') LIMIT 1");$q->execute(['hash'=>hash('sha256',$token)]);$canReset=(bool)$q->fetchColumn();}
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token. Reload and try again.');}
 $password=(string)($_POST['password']??'');$confirm=(string)($_POST['confirm_password']??'');
 if(!$canReset)$error='This reset link is invalid or has expired. Request a new link.';
 elseif(strlen($password)<12)$error='Your new password must be at least 12 characters.';
 elseif($password!==$confirm)$error='The passwords do not match.';
 else{
  $pdo=db();$pdo->beginTransaction();
  try{
   $q=$pdo->prepare("SELECT t.id,t.user_id FROM password_reset_tokens t JOIN users u ON u.id=t.user_id WHERE t.token_hash=:hash AND t.used_at IS NULL AND t.expires_at>datetime('now') AND u.status='active' LIMIT 1");
   $q->execute(['hash'=>hash('sha256',$token)]);$row=$q->fetch();
   if(!$row){$pdo->rollBack();$error='This reset link is invalid or has expired. Request a new link.';}
   else{
    $q=$pdo->prepare("UPDATE users SET password_hash=:hash,updated_at=datetime('now') WHERE id=:id AND status='active'");$q->execute(['hash'=>password_hash($password,PASSWORD_DEFAULT),'id'=>(int)$row['user_id']]);
    $q=$pdo->prepare("UPDATE password_reset_tokens SET used_at=datetime('now') WHERE user_id=:id AND used_at IS NULL");$q->execute(['id'=>(int)$row['user_id']]);
    $pdo->commit();audit_log((int)$row['user_id'],'auth.password_reset_completed','user',(string)$row['user_id']);$complete=true;
   }
  }catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();throw $ex;}
 }
}elseif(!$canReset){
 $error='This reset link is invalid or has expired. Request a new link.';
}
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Choose new password · Staff Sales</title><link rel="stylesheet" href="<?= e(app_url('assets/css/app.css')) ?>"></head><body class="auth-page"><main class="auth-shell"><a class="brand" href="<?= e(app_url('login.php')) ?>"><span class="brand-mark">SS</span><span><strong>Staff Sales</strong><small>Internal auction portal</small></span></a><section class="auth-panel"><p class="eyebrow">ACCOUNT RECOVERY</p><h1>Choose a new password.</h1>
<?php if($complete): ?><div class="notice"><span class="notice-dot"></span><div><strong>Password updated</strong><p>You can now sign in with your new password.</p></div></div><a class="button button-primary button-wide" href="<?= e(app_url('login.php')) ?>">Return to sign in</a><?php else: ?><?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?><?php if($canReset): ?><form method="post" action="<?= e(app_url('reset-password.php')) ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="token" value="<?= e($token) ?>"><label for="password">New password</label><input id="password" name="password" type="password" minlength="12" required autocomplete="new-password"><label for="confirm_password">Confirm new password</label><input id="confirm_password" name="confirm_password" type="password" minlength="12" required autocomplete="new-password"><p class="form-help">Use at least 12 characters.</p><button class="button button-primary button-wide" type="submit">Update password →</button></form><?php endif; ?><p class="fine-print"><a href="<?= e(app_url('reset-request.php')) ?>">Request another reset link</a> · <a href="<?= e(app_url('login.php')) ?>">Back to sign in</a></p><?php endif; ?></section><footer>Staff Sales Auction Portal · internal development release</footer></main></body></html>
