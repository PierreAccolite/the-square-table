<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
$error='';$success='';
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
  elseif(($startsRaw!==''&&$starts===null)||($endsRaw!==''&&$ends===null)){ $error='Please check the date and time values.'; }
  elseif($starts!==null&&$ends!==null&&$ends<=$starts){$error='The closing time must be later than the opening time.';}
  else{
   $q=db()->prepare("INSERT INTO auctions(title,description,status,starts_at,ends_at,created_by) VALUES(:title,:description,'draft',:starts,:ends,:actor)");
   $q->execute(['title'=>$title,'description'=>$description!==''?$description:null,'starts'=>$starts,'ends'=>$ends,'actor'=>$user['id']]);
   $id=(int)db()->lastInsertId();audit_log((int)$user['id'],'auction.created','auction',(string)$id,['title'=>$title]);
   redirect_to('admin/items.php?auction='.$id.'&created=1');
  }
 }
}
$auctions=db()->query("SELECT a.id,a.title,a.status,a.starts_at,a.ends_at,a.created_at,COUNT(i.id) AS item_count FROM auctions a LEFT JOIN auction_items i ON i.auction_id=a.id GROUP BY a.id ORDER BY a.created_at DESC,a.id DESC")->fetchAll();
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Auctions · Staff Sales</title><link rel="stylesheet" href="<?= e(app_url('assets/css/app.css')) ?>"></head><body>
<header class="topbar"><a class="brand" href="<?= e(app_url('index.php')) ?>"><span class="brand-mark">SS</span><span><strong>Staff Sales</strong><small>Administration</small></span></a><div class="topbar-actions"><a class="text-link" href="<?= e(app_url('admin/index.php')) ?>">Control centre</a><form method="post" action="<?= e(app_url('logout.php')) ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><button class="button button-quiet" type="submit">Sign out</button></form></div></header>
<main class="page-shell"><section class="welcome"><div><p class="eyebrow">AUCTION SETUP</p><h1>Auctions</h1><p class="muted">Create an auction and build its item catalogue. New auctions remain drafts until a later publishing step.</p></div><div class="welcome-icon">⌁</div></section>
<?php if(isset($_GET['created'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Auction created</strong><p>Add the first item to its catalogue below.</p></div></div><?php endif; ?>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">NEW AUCTION</p><h2>Create an auction draft</h2></div></div><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="create"><label for="title">Auction title</label><input id="title" name="title" maxlength="180" required placeholder="e.g. October Staff Sale"><label for="description">Description <span class="muted">(optional)</span></label><textarea id="description" name="description" rows="3" maxlength="10000" placeholder="Sale details, collection arrangements and conditions"></textarea><div class="form-grid"><div><label for="starts_at">Planned opening</label><input id="starts_at" name="starts_at" type="datetime-local"></div><div><label for="ends_at">Planned closing</label><input id="ends_at" name="ends_at" type="datetime-local"></div></div><p class="form-help">Times use the server's configured timezone: <?= e((string)app_config('timezone')) ?>. These dates are planning information only; live opening and closing automation is not enabled yet.</p><button class="button button-primary" type="submit">Create draft →</button></form></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">YOUR CATALOGUES</p><h2>Created auctions</h2></div><span class="status-pill"><?= count($auctions) ?> total</span></div>
<?php if(!$auctions): ?><div class="empty-state"><strong>No auctions yet</strong><p>Create your first draft using the form above.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><th>Auction</th><th>Status</th><th>Items</th><th>Planned opening</th><th>Planned closing</th><th></th></tr></thead><tbody><?php foreach($auctions as $a): ?><tr><td><strong><?= e($a['title']) ?></strong><small class="table-sub"><?= e($a['description']??'') ?></small></td><td><span class="status-pill"><?= e(ucfirst($a['status'])) ?></span></td><td><?= (int)$a['item_count'] ?></td><td><?= e($a['starts_at']??'Not set') ?></td><td><?= e($a['ends_at']??'Not set') ?></td><td><a class="text-link" href="<?= e(app_url('admin/items.php?auction='.(int)$a['id'])) ?>">Manage items →</a></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?></section>
<p><a class="text-link" href="<?= e(app_url('admin/index.php')) ?>">← Return to administration</a></p><footer class="page-footer">Staff Sales Auction Portal · v0.2 development</footer></main></body></html>