<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
$q=db()->prepare('SELECT id,username,display_name,employee_number,email,role,status,last_login_at,password_hash FROM users WHERE id=:id LIMIT 1');
$q->execute(['id'=>(int)$user['id']]);$account=$q->fetch();
if(!$account){$_SESSION=[];redirect_to('login.php');}
$error='';
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $action=(string)($_POST['action']??'');$current=(string)($_POST['current_password']??'');
 if(!password_verify($current,(string)$account['password_hash']))$error='Your current password is incorrect.';
 elseif($action==='profile'){
  $display=trim((string)($_POST['display_name']??''));$email=trim((string)($_POST['email']??''));
  if($display===''||mb_strlen($display)>160)$error='Display name must be between 1 and 160 characters.';
  elseif($email!==''&&(!filter_var($email,FILTER_VALIDATE_EMAIL)||strlen($email)>254))$error='Enter a valid email address, or leave it blank.';
  else{$pdo=db();$pdo->beginTransaction();try{$emailChanged=$email!==(string)($account['email']??'');$q=$pdo->prepare("UPDATE users SET display_name=:display,email=:email,updated_at=datetime('now') WHERE id=:id");$q->execute(['display'=>$display,'email'=>$email!==''?$email:null,'id'=>(int)$user['id']]);if($emailChanged){$q=$pdo->prepare("UPDATE password_reset_tokens SET used_at=datetime('now') WHERE user_id=:id AND used_at IS NULL");$q->execute(['id'=>(int)$user['id']]);}$pdo->commit();$_SESSION['user']['display_name']=$display;audit_log((int)$user['id'],'user.self_profile_updated','user',(string)$user['id'],['email_changed'=>$emailChanged]);redirect_to('account.php?profile_saved=1');}catch(PDOException $ex){if($pdo->inTransaction())$pdo->rollBack();if(str_contains(strtolower($ex->getMessage()),'unique'))$error='That email address is already assigned to another account.';else throw $ex;}catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();throw $ex;}}
 }elseif($action==='password'){
  $new=(string)($_POST['new_password']??'');$confirm=(string)($_POST['confirm_password']??'');
  if(strlen($new)<12)$error='Your new password must be at least 12 characters.';
  elseif($new!==$confirm)$error='The new passwords do not match.';
  elseif(password_verify($new,(string)$account['password_hash']))$error='Choose a password different from your current password.';
  else{$pdo=db();$pdo->beginTransaction();try{$q=$pdo->prepare("UPDATE users SET password_hash=:hash,updated_at=datetime('now') WHERE id=:id");$q->execute(['hash'=>password_hash($new,PASSWORD_DEFAULT),'id'=>(int)$user['id']]);$q=$pdo->prepare("UPDATE password_reset_tokens SET used_at=datetime('now') WHERE user_id=:id AND used_at IS NULL");$q->execute(['id'=>(int)$user['id']]);$pdo->commit();session_regenerate_id(true);audit_log((int)$user['id'],'auth.password_changed','user',(string)$user['id']);redirect_to('account.php?password_saved=1');}catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();throw $ex;}}
 }else{$error='Unknown account action.';}
}
require dirname(__DIR__).'/app/layout.php';render_page_start('My account','account',$user);
?>
<section class="welcome"><div><p class="eyebrow">SELF-SERVICE</p><h1>My account</h1><p class="muted">Update your contact details and manage your own password.</p></div><div class="welcome-icon">♙</div></section>
<?php if(isset($_GET['profile_saved'])): ?><div class="notice" role="status"><span class="notice-dot"></span><div><strong>Profile updated</strong></div></div><?php endif; ?>
<?php if(isset($_GET['password_saved'])): ?><div class="notice" role="status"><span class="notice-dot"></span><div><strong>Password changed</strong><p>Outstanding password-reset links have been invalidated.</p></div></div><?php endif; ?>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">ACCOUNT DETAILS</p><h2>Your profile</h2></div><span class="status-pill"><?= e(ucfirst((string)$account['status'])) ?></span></div>
<div class="form-grid"><div><label>Username</label><input value="<?= e($account['username']) ?>" readonly></div><div><label>Employee number</label><input value="<?= e($account['employee_number']??'Not set') ?>" readonly></div><div><label>Access role</label><input value="<?= e(ucfirst((string)$account['role'])) ?>" readonly></div><div><label>Last sign-in</label><input value="<?= e($account['last_login_at']??'Not recorded') ?>" readonly></div></div>
<p class="form-help">Username, employee number, role and status are managed by an administrator.</p>
<form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="profile"><label for="display_name">Display name</label><input id="display_name" name="display_name" maxlength="160" required value="<?= e($account['display_name']) ?>" autocomplete="name"><label for="email">Email address</label><input id="email" name="email" type="email" maxlength="254" value="<?= e($account['email']??'') ?>" autocomplete="email"><p class="form-help">Your email is used for password recovery and auction-result notifications.</p><label for="profile-password">Confirm with current password</label><input id="profile-password" name="current_password" type="password" required autocomplete="current-password"><button class="button button-primary" type="submit">Save profile →</button></form></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">SECURITY</p><h2>Change password</h2></div></div><p class="muted">Use at least 12 characters. Your current password is required.</p><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="password"><label for="current-password">Current password</label><input id="current-password" name="current_password" type="password" required autocomplete="current-password"><label for="new-password">New password</label><input id="new-password" name="new_password" type="password" minlength="12" required autocomplete="new-password"><label for="confirm-password">Confirm new password</label><input id="confirm-password" name="confirm_password" type="password" minlength="12" required autocomplete="new-password"><button class="button button-primary" type="submit">Change password →</button></form></section>
<section class="content-panel"><h2>Account recovery</h2><p class="muted">If you are signed out and cannot remember your password, request a one-time reset link to your registered email address.</p><a class="button button-quiet" href="<?= e(app_url('reset-request.php')) ?>">Open password recovery →</a></section>
<?php render_page_end(); ?>