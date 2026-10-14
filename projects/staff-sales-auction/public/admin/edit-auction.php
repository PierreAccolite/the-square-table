<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
$id=filter_input(INPUT_GET,'id',FILTER_VALIDATE_INT) ?: (int)($_POST['auction_id']??0);
$q=db()->prepare('SELECT * FROM auctions WHERE id=:id');$q->execute(['id'=>$id]);$auction=$q->fetch();
if(!$auction){http_response_code(404);exit('Auction not found.');}
$error='';
function edit_auction_datetime(string $value):?string{$value=trim($value);if($value==='')return null;$dt=DateTimeImmutable::createFromFormat('!Y-m-d\\TH:i',$value);return $dt?$dt->format('Y-m-d H:i:s'):null;}
function html_datetime_value(?string $value):string{if(!$value)return '';try{return (new DateTimeImmutable($value))->format('Y-m-d\\TH:i');}catch(Throwable $e){return '';}}
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $title=trim((string)($_POST['title']??''));$description=trim((string)($_POST['description']??''));$startRaw=(string)($_POST['starts_at']??'');$endRaw=(string)($_POST['ends_at']??'');$start=edit_auction_datetime($startRaw);$end=edit_auction_datetime($endRaw);
 if(!in_array($auction['status'],['draft','scheduled'],true))$error='Only draft or scheduled auctions can be edited in this release.';
 elseif($title===''||mb_strlen($title)>180)$error='Enter an auction title of 1–180 characters.';
 elseif(mb_strlen($description)>10000)$error='Description is too long.';
 elseif($start===null||$end===null)$error='Both opening and closing times are required.';
 elseif($end<=$start)$error='Closing time must be later than opening time.';
 else{$q=db()->prepare("UPDATE auctions SET title=:title,description=:description,starts_at=:starts,ends_at=:ends,updated_at=datetime('now') WHERE id=:id AND status IN ('draft','scheduled')");$q->execute(['title'=>$title,'description'=>$description!==''?$description:null,'starts'=>$start,'ends'=>$end,'id'=>$id]);audit_log((int)$user['id'],'auction.updated','auction',(string)$id,['status'=>$auction['status']]);redirect_to('admin/edit-auction.php?id='.$id.'&saved=1');}
 $q=db()->prepare('SELECT * FROM auctions WHERE id=:id');$q->execute(['id'=>$id]);$auction=$q->fetch();
}
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Edit auction','auctions',$user);
?>
<section class="welcome"><div><p class="eyebrow">AUCTION MANAGEMENT</p><h1>Edit auction</h1><p class="muted">Update the sale title, description and planned dates.</p></div><div class="welcome-icon">✎</div></section>
<?php if(isset($_GET['saved'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Auction updated</strong><p>Your changes have been saved.</p></div></div><?php endif; ?><?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">AUCTION DETAILS</p><h2><?= e($auction['title']) ?></h2></div><span class="status-pill"><?= e(ucfirst($auction['status'])) ?></span></div><?php if(!in_array($auction['status'],['draft','scheduled'],true)): ?><div class="empty-state"><strong>This auction is locked</strong><p>Only draft or scheduled auctions can be edited in this release.</p></div><?php else: ?><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="auction_id" value="<?= (int)$id ?>"><label for="title">Auction title</label><input id="title" name="title" maxlength="180" required value="<?= e($auction['title']) ?>"><label for="description">Description</label><textarea id="description" name="description" rows="4" maxlength="10000"><?= e($auction['description']??'') ?></textarea><div class="form-grid"><div><label for="starts_at">Planned opening</label><input id="starts_at" name="starts_at" type="datetime-local" required value="<?= e(html_datetime_value($auction['starts_at'])) ?>"></div><div><label for="ends_at">Planned closing</label><input id="ends_at" name="ends_at" type="datetime-local" required value="<?= e(html_datetime_value($auction['ends_at'])) ?>"></div></div><p class="form-help">Configured timezone: <?= e((string)app_config('timezone')) ?>. Changing these dates does not enable live bidding or automated opening/closing.</p><button class="button button-primary" type="submit">Save auction →</button></form><?php endif; ?></section><p><a class="text-link" href="<?= e(app_url('admin/auctions.php')) ?>">← Return to auctions</a></p>
<?php render_page_end(); ?>