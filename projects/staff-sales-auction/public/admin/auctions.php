<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
$error='';
function local_datetime_value(string $value):?string{
 $value=trim($value);if($value==='')return null;
 $dt=DateTimeImmutable::createFromFormat('!Y-m-d\\TH:i', $value);
 return $dt ? $dt->format('Y-m-d H:i:s') : null;
}
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token. Reload and try again.');}
 $action=(string)($_POST['action']??'');
 if($action==='create'){
  $title=trim((string)($_POST['title']??''));$description=trim((string)($_POST['description']??''));
  $startsRaw=(string)($_POST['starts_at']??'');$endsRaw=(string)($_POST['ends_at']??'');
  $starts=local_datetime_value($startsRaw);$ends=local_datetime_value($endsRaw);
  if($title===''||mb_strlen($title)>180){$error='Enter an auction title of 1–180 characters.';}
  elseif(mb_strlen($description)>10000){$error='The description is too long.';}
  elseif(($startsRaw!==''&&$starts===null)||($endsRaw!==''&&$ends===null)){$error='Please check the date and time values.';}
  elseif($starts!==null&&$ends!==null&&$ends<=$starts){$error='The closing time must be later than the opening time.';}
  else{
   $q=db()->prepare("INSERT INTO auctions(title,description,status,starts_at,ends_at,created_by) VALUES(:title,:description,'draft',:starts,:ends,:actor)");
   $q->execute(['title'=>$title,'description'=>$description!==''?$description:null,'starts'=>$starts,'ends'=>$ends,'actor'=>$user['id']]);
   $id=(int)db()->lastInsertId();audit_log((int)$user['id'],'auction.created','auction',(string)$id,['title'=>$title]);
   redirect_to('admin/items.php?auction='.$id.'&created=1');
  }
 }elseif($action==='schedule'){
  $id=(int)($_POST['auction_id']??0);
  $q=db()->prepare("SELECT id,title,status,starts_at,ends_at FROM auctions WHERE id=:id");$q->execute(['id'=>$id]);$a=$q->fetch();
  $q=db()->prepare("SELECT COUNT(*) FROM auction_items WHERE auction_id=:id AND status='published'");$q->execute(['id'=>$id]);$published=(int)$q->fetchColumn();
  if(!$a||$a['status']!=='draft'){$error='Only a draft auction can be scheduled.';}
  elseif(empty($a['starts_at'])||empty($a['ends_at'])){$error='Set both planned opening and closing times before scheduling.';}
  elseif((string)$a['ends_at']<=(string)$a['starts_at']){$error='The closing time must be later than the opening time.';}
  elseif($published<1){$error='Publish at least one catalogue item before scheduling this auction.';}
  else{
   $q=db()->prepare("UPDATE auctions SET status='scheduled',updated_at=strftime('%Y-%m-%d %H:%M:%S','now') WHERE id=:id AND status='draft'");$q->execute(['id'=>$id]);
   audit_log((int)$user['id'],'auction.scheduled','auction',(string)$id,['title'=>$a['title'],'published_items'=>$published]);
   redirect_to('admin/auctions.php?scheduled=1');
  }
 }
}
$auctions=db()->query("SELECT a.id,a.title,a.description,a.status,a.starts_at,a.ends_at,a.created_at,COUNT(i.id) AS item_count,SUM(CASE WHEN i.status='published' THEN 1 ELSE 0 END) AS published_count FROM auctions a LEFT JOIN auction_items i ON i.auction_id=a.id GROUP BY a.id ORDER BY a.created_at DESC,a.id DESC")->fetchAll();
?>
<?php require dirname(__DIR__,2).'/app/layout.php'; render_page_start('Auctions','auctions',$user); ?><section class="welcome"><div><p class="eyebrow">AUCTION SETUP</p><h1>Auctions</h1><p class="muted">Create a draft, prepare the catalogue, then schedule it for staff to view. Scheduling does not enable bidding.</p></div><div class="welcome-icon">⌁</div></section>
<?php if(isset($_GET['created'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Auction created</strong><p>Add the first item to its catalogue below.</p></div></div><?php endif; ?>
<?php if(isset($_GET['scheduled'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Auction scheduled</strong><p>Published catalogue items are now visible to signed-in staff. Bidding remains disabled.</p></div></div><?php endif; ?>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">NEW AUCTION</p><h2>Create an auction draft</h2></div></div><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="create"><label for="title">Auction title</label><input id="title" name="title" maxlength="180" required placeholder="e.g. October Staff Sale"><label for="description">Description <span class="muted">(optional)</span></label><textarea id="description" name="description" rows="3" maxlength="10000" placeholder="Sale details, collection arrangements and conditions"></textarea><div class="form-grid"><div><label for="starts_at">Planned opening</label><input id="starts_at" name="starts_at" type="datetime-local" required></div><div><label for="ends_at">Planned closing</label><input id="ends_at" name="ends_at" type="datetime-local" required></div></div><p class="form-help">Times use the server's configured timezone: <?= e((string)app_config('timezone')) ?>. Scheduling publishes the catalogue only; live bidding and automatic opening/closing are not implemented yet.</p><button class="button button-primary" type="submit">Create draft →</button></form></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">YOUR CATALOGUES</p><h2>Created auctions</h2></div><span class="status-pill"><?= count($auctions) ?> total</span></div>
<?php if(!$auctions): ?><div class="empty-state"><strong>No auctions yet</strong><p>Create your first draft using the form above.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><th>Auction</th><th>Status</th><th>Items</th><th>Planned opening</th><th>Planned closing</th><th>Actions</th></tr></thead><tbody><?php foreach($auctions as $a): ?><tr><td><strong><?= e($a['title']) ?></strong><small class="table-sub"><?= e($a['description']??'') ?></small></td><td><span class="status-pill"><?= e(ucfirst($a['status'])) ?></span></td><td><?= (int)$a['item_count'] ?> <small class="table-sub"><?= (int)($a['published_count']??0) ?> published</small></td><td><?= e($a['starts_at']??'Not set') ?></td><td><?= e($a['ends_at']??'Not set') ?></td><td><a class="text-link" href="<?= e(app_url('admin/items.php?auction='.(int)$a['id'])) ?>">Manage items</a><?php if($a['status']==='draft'): ?><form method="post" class="row-form" onsubmit="return confirm('Schedule this auction and make its published catalogue visible to staff? Bidding will remain disabled.');"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="schedule"><input type="hidden" name="auction_id" value="<?= (int)$a['id'] ?>"><button class="button button-quiet" type="submit">Schedule</button></form><?php endif; ?></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?></section>
<p><a class="text-link" href="<?= e(app_url('admin/index.php')) ?>">← Return to administration</a></p><?php render_page_end(); ?>