<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
if(current_user()!==null)redirect_to('index.php');
$sent=false;
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token. Reload and try again.');}
 $email=trim((string)($_POST['email']??''));
 if(strlen($email)<=254&&filter_var($email,FILTER_VALIDATE_EMAIL)){
  $q=db()->prepare("SELECT id,display_name,email FROM users WHERE email=:email COLLATE NOCASE AND status='active' LIMIT 1");$q->execute(['email'=>$email]);$account=$q->fetch();
  if($account){
   $q=db()->prepare("SELECT COUNT(*) FROM password_reset_tokens WHERE user_id=:id AND created_at>=datetime('now','-1 hour')");$q->execute(['id'=>(int)$account['id']]);
   if((int)$q->fetchColumn()<3){
    $token=bin2hex(random_bytes(32));$hash=hash('sha256',$token);
    $pdo=db();$pdo->beginTransaction();
    try{$q=$pdo->prepare("UPDATE password_reset_tokens SET used_at=datetime('now') WHERE user_id=:id AND used_at IS NULL");$q->execute(['id'=>(int)$account['id']]);
     $q=$pdo->prepare("INSERT INTO password_reset_tokens(user_id,token_hash,expires_at) VALUES(:id,:hash,datetime('now','+30 minutes'))");$q->execute(['id'=>(int)$account['id'],'hash'=>$hash]);$pdo->commit();
    }catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();throw $ex;}
    $link=app_url('reset-password.php?token='.rawurlencode($token));
    $body="Hello ".$account['display_name'].",\n\nA password reset was requested for your Staff Sales Auction Portal account. Use this one-time link within 30 minutes:\n\n".$link."\n\nIf you did not request this, you can ignore this message. Your password will not change unless the link is used.\n";
    if(!send_app_email((string)$account['email'],'Staff Sales password reset',$body))error_log('Staff Sales password reset email could not be handed to the mail transport for user ID '.(int)$account['id']);
    else audit_log((int)$account['id'],'auth.password_reset_requested','user',(string)$account['id']);
   }
  }
 }
 $sent=true;
}
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Reset password · Staff Sales</title><link rel="stylesheet" href="<?= e(app_url('assets/css/app.css')) ?>"></head><body class="auth-page"><main class="auth-shell"><a class="brand" href="<?= e(app_url('login.php')) ?>"><span class="brand-mark">SS</span><span><strong>Staff Sales</strong><small>Internal auction portal</small></span></a><section class="auth-panel"><p class="eyebrow">ACCOUNT RECOVERY</p><h1>Reset your password.</h1>
<?php if($sent): ?><div class="notice"><span class="notice-dot"></span><div><strong>Check your email</strong><p>If an active account matches that email address, and mail delivery is configured, a reset link will be sent.</p></div></div><a class="button button-primary button-wide" href="<?= e(app_url('login.php')) ?>">Return to sign in</a><?php else: ?><p class="muted">Enter the email address registered to your staff account.</p><form method="post" action="<?= e(app_url('reset-request.php')) ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><label for="email">Email address</label><input id="email" name="email" type="email" maxlength="254" required autocomplete="email"><button class="button button-primary button-wide" type="submit">Send reset link →</button></form><p class="fine-print">For security, this page does not confirm whether an account exists.</p><?php endif; ?></section><footer>Staff Sales Auction Portal · internal development release</footer></main></body></html>
