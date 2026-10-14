<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();$error='';
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $id=(int)($_POST['notification_id']??0);
 if($id===0){$q=db()->prepare('UPDATE notifications SET read_at=datetime(\'now\') WHERE user_id=:uid AND read_at IS NULL');$q->execute(['uid'=>$user['id']]);}
 else{$q=db()->prepare('UPDATE notifications SET read_at=datetime(\'now\') WHERE id=:id AND user_id=:uid AND read_at IS NULL');$q->execute(['id'=>$id,'uid'=>$user['id']]);}
 redirect_to('notifications.php');
}
$q=db()->prepare('SELECT id,notification_type,title,message,read_at,created_at FROM notifications WHERE user_id=:uid ORDER BY created_at DESC,id DESC LIMIT 100');$q->execute(['uid'=>$user['id']]);$notifications=$q->fetchAll();
require dirname(__DIR__).'/app/layout.php';render_page_start('Notifications','notifications',$user);
?>
<section class="welcome"><div><p class="eyebrow">YOUR ACCOUNT</p><h1>Notifications</h1><p class="muted">Auction updates and account messages assigned to your staff account.</p></div><div class="welcome-icon">♧</div></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">INBOX</p><h2>Your notifications</h2></div><?php if(array_filter($notifications,static fn($n)=>$n['read_at']===null)): ?><form method="post"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><button class="button button-quiet" type="submit">Mark all read</button></form><?php endif; ?></div><?php if(!$notifications): ?><div class="empty-state"><strong>You're all caught up</strong><p>When auction notifications are enabled, they will appear here.</p></div><?php else: ?><div class="notification-list"><?php foreach($notifications as $n): ?><article class="notification-row<?= $n['read_at']===null?' is-unread':'' ?>"><div class="notification-dot"></div><div class="notification-content"><div class="notification-heading"><strong><?= e($n['title']) ?></strong><span class="status-pill"><?= e($n['read_at']===null?'Unread':'Read') ?></span></div><p><?= nl2br(e($n['message'])) ?></p><small><?= e($n['created_at']) ?> · <?= e(str_replace('_',' ',$n['notification_type'])) ?></small></div><?php if($n['read_at']===null): ?><form method="post"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="notification_id" value="<?= (int)$n['id'] ?>"><button class="button button-quiet" type="submit">Mark read</button></form><?php endif; ?></article><?php endforeach; ?></div><?php endif; ?></section>
<?php render_page_end(); ?>