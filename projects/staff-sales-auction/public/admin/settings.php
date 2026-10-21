<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
$defaults=[
 'mail_from'=>trim((string)app_config('mail_from')),
 'mail_from_name'=>'Staff Sales Auction Portal',
 'mail_enabled'=>'1',
 'winner_instructions'=>'Please contact the staff sales team to arrange payment and collection. Do not make payment until you receive the official instructions from the authorised team.'
];
try{$q=db()->query("SELECT setting_key,setting_value FROM app_settings WHERE setting_key IN ('mail_from','mail_from_name','mail_enabled','winner_instructions')");foreach($q->fetchAll() as $row)$defaults[(string)$row['setting_key']]=(string)$row['setting_value'];}catch(Throwable $ignored){}
$error='';$success='';
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $action=(string)($_POST['action']??'save');
 $from=trim((string)($_POST['mail_from']??''));$fromName=trim((string)($_POST['mail_from_name']??''));$enabled=isset($_POST['mail_enabled'])?'1':'0';$instructions=trim((string)($_POST['winner_instructions']??$defaults['winner_instructions']));
 if(!filter_var($from,FILTER_VALIDATE_EMAIL))$error='Enter a valid sender email address.';
 elseif($fromName===''||mb_strlen($fromName)>80)$error='Sender name must be between 1 and 80 characters.';
 elseif(mb_strlen($instructions)>4000)$error='Winner payment/collection instructions must be no more than 4,000 characters.';
 else{
  $pdo=db();$pdo->beginTransaction();
  try{
   $up=$pdo->prepare("INSERT INTO app_settings(setting_key,setting_value,updated_by,updated_at) VALUES(:key,:value,:actor,datetime('now')) ON CONFLICT(setting_key) DO UPDATE SET setting_value=excluded.setting_value,updated_by=excluded.updated_by,updated_at=excluded.updated_at");
   foreach(['mail_from'=>$from,'mail_from_name'=>$fromName,'mail_enabled'=>$enabled,'winner_instructions'=>$instructions] as $key=>$value)$up->execute(['key'=>$key,'value'=>$value,'actor'=>(int)$user['id']]);
   $pdo->commit();$defaults=['mail_from'=>$from,'mail_from_name'=>$fromName,'mail_enabled'=>$enabled,'winner_instructions'=>$instructions];
   audit_log((int)$user['id'],'mail.settings_updated','system','mail',['enabled'=>$enabled==='1','sender'=>$from]);
  }catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();throw $ex;}
  if($action==='test'){
   $recipient=trim((string)($_POST['test_recipient']??''));
   if(!filter_var($recipient,FILTER_VALIDATE_EMAIL))$error='Settings saved, but enter a valid test recipient email address.';
   elseif($enabled!=='1')$error='Settings saved, but mail delivery is disabled. Enable it before sending a test.';
   else{
    $body="This is a test message from the Staff Sales Auction Portal.\n\nIf you received this email, PHP mail() successfully handed the message to the configured server mail transport.\n\nSender: ".$fromName." <".$from.">\nSent at: ".(new DateTimeImmutable('now'))->format('Y-m-d H:i:s');
    if(send_app_email($recipient,'Staff Sales mail transport test',$body)){$success='Test email was handed to the server mail transport. Check the recipient inbox and Postfix logs to confirm delivery.';audit_log((int)$user['id'],'mail.test_submitted','system','mail',['recipient'=>$recipient]);}
    else $error='The test email could not be handed to the mail transport. Check whether local Postfix is running and review the mail logs.';
   }
  }else $success='Mail settings saved.';
 }
}
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Email settings','settings',$user);
?>
<section class="welcome"><div><p class="eyebrow">ADMIN SETTINGS</p><h1>Email settings</h1><p class="muted">Configure the sender identity used for password resets and auction result notifications.</p></div><div class="welcome-icon">✉</div></section>
<div class="notice"><span class="notice-dot"></span><div><strong>Uses the server's existing Postfix mail transport</strong><p>This portal uses PHP's built-in mail() function, which hands messages to the mail transport configured on the web server. If Postfix is running locally, relay host, port, TLS and credentials are configured in Postfix—not in this page. No SMTP password is stored in the portal database.</p></div></div>
<?php if($success!==''): ?><div class="notice" role="status"><span class="notice-dot"></span><div><?= e($success) ?></div></div><?php endif; ?><?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">SENDER CONFIGURATION</p><h2>Mail delivery</h2></div><span class="status-pill"><?= $defaults['mail_enabled']==='1'?'Enabled':'Disabled' ?></span></div><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="save"><div class="form-grid"><div><label for="mail_from_name">Sender display name</label><input id="mail_from_name" name="mail_from_name" maxlength="80" required value="<?= e($defaults['mail_from_name']) ?>"></div><div><label for="mail_from">Sender email address</label><input id="mail_from" name="mail_from" type="email" maxlength="254" required value="<?= e($defaults['mail_from']) ?>"></div></div><label class="checkbox-line"><input type="checkbox" name="mail_enabled" value="1" <?= $defaults['mail_enabled']==='1'?'checked':'' ?>> Enable portal email notifications</label><label for="winner_instructions">Winner payment and collection instructions</label><textarea id="winner_instructions" name="winner_instructions" rows="5" maxlength="4000"><?= e($defaults['winner_instructions']) ?></textarea><p class="form-help">Included only in the winning employee’s result email. Do not include passwords or sensitive account credentials. Payment processing is not handled by this portal.</p><p class="form-help">Disabling mail stops password reset emails and auction winner emails. In-app notifications continue to work.</p><button class="button button-primary" type="submit">Save mail settings →</button></form></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">DELIVERY TEST</p><h2>Send a test email</h2></div></div><p class="muted">This sends a real email to the address entered below. It confirms that PHP handed the message to the server's mail transport; it does not guarantee final delivery.</p><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="test"><input type="hidden" name="mail_from" value="<?= e($defaults['mail_from']) ?>"><input type="hidden" name="mail_from_name" value="<?= e($defaults['mail_from_name']) ?>"><input type="hidden" name="winner_instructions" value="<?= e($defaults['winner_instructions']) ?>"><?php if($defaults['mail_enabled']==='1'): ?><input type="hidden" name="mail_enabled" value="1"><?php endif; ?><label for="test_recipient">Test recipient</label><input id="test_recipient" name="test_recipient" type="email" required maxlength="254" placeholder="your.name@company.example"><button class="button button-secondary" type="submit">Save and send test →</button></form></section>
<section class="content-panel"><h2>Before enabling password reset</h2><ul><li>Set a sender address accepted by your Postfix configuration.</li><li>Confirm the web server can submit mail to Postfix.</li><li>Add a unique email address to each employee account under User Management.</li><li>Test a password reset with a controlled account and check Postfix logs.</li></ul><p class="form-help">Reset links expire after 30 minutes and can only be used once. The reset request page does not reveal whether an account exists.</p></section>
<?php render_page_end(); ?>
