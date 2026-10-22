<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_permission('procurement','custodian');
$error='';$success='';
function next_lot_number(PDO $pdo,int $auctionId):string{
 $q=$pdo->prepare('SELECT lot_number FROM auction_items WHERE auction_id=:id');$q->execute(['id'=>$auctionId]);
 $max=0;$width=4;
 foreach($q->fetchAll(PDO::FETCH_COLUMN) as $existing){
  $existing=(string)$existing;
  if(preg_match('/^\\d+$/',$existing)){
   $number=(int)$existing;
   if($number>$max){$max=$number;$width=max(4,strlen($existing));}
   elseif($number===$max)$width=max($width,strlen($existing));
  }
 }
 return str_pad((string)($max+1),$width,'0',STR_PAD_LEFT);
}
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $resultId=(int)($_POST['result_id']??0);$targetAuction=(int)($_POST['target_auction_id']??0);
 $pdo=db();$pdo->exec('BEGIN IMMEDIATE');
 try{
  $q=$pdo->prepare("SELECT r.id AS result_id,r.auction_id,r.pending_disposition,r.result_status,i.*,a.results_status AS source_results_status FROM auction_results r JOIN auction_items i ON i.id=r.auction_item_id JOIN auctions a ON a.id=r.auction_id WHERE r.id=:id");$q->execute(['id'=>$resultId]);$source=$q->fetch();
  if(!$source||$source['result_status']!=='unsold'||$source['pending_disposition']!=='pending'||$source['source_results_status']!=='released')throw new RuntimeException('This item is no longer available in Pending Items.');
  $q=$pdo->prepare("SELECT id,title FROM auctions WHERE id=:id AND status='draft'");$q->execute(['id'=>$targetAuction]);$target=$q->fetch();
  if(!$target)throw new RuntimeException('Choose a destination auction that is still a draft.');
  $lot=next_lot_number($pdo,$targetAuction);$now=date('Y-m-d H:i:s');
  $q=$pdo->prepare("INSERT INTO auction_items(auction_id,lot_number,title,description,condition_notes,image_path,opening_price_cents,minimum_increment_cents,status,created_at,updated_at,category,buyout_enabled,buyout_price_cents,source_item_id) VALUES(:auction,:lot,:title,:description,:condition,:image,:opening,:increment,'draft',:now,:now,:category,:buyout_enabled,:buyout_price,:source)");
  $q->execute(['auction'=>$targetAuction,'lot'=>$lot,'title'=>$source['title'],'description'=>$source['description'],'condition'=>$source['condition_notes'],'image'=>$source['image_path'],'opening'=>$source['opening_price_cents'],'increment'=>$source['minimum_increment_cents'],'now'=>$now,'category'=>$source['category']??'Uncategorised','buyout_enabled'=>$source['buyout_enabled']??0,'buyout_price'=>$source['buyout_price_cents'],'source'=>$source['id']]);
  $newItemId=(int)$pdo->lastInsertId();
  $q=$pdo->prepare("UPDATE auction_results SET pending_disposition='relisted',relisted_item_id=:new_item WHERE id=:id AND pending_disposition='pending'");$q->execute(['new_item'=>$newItemId,'id'=>$resultId]);
  if($q->rowCount()!==1)throw new RuntimeException('Pending-item state changed. Reload and try again.');
  audit_log((int)$user['id'],'auction.unsold_item_relisted','auction_item',(string)$newItemId,['source_item_id'=>(int)$source['id'],'source_result_id'=>$resultId,'source_auction_id'=>(int)$source['auction_id'],'target_auction_id'=>$targetAuction,'new_lot_number'=>$lot]);
  $pdo->exec('COMMIT');$success='Item copied into '.$target['title'].' as lot '.$lot.'. The original auction and bidding history remain unchanged.';
 }catch(Throwable $e){try{$pdo->exec('ROLLBACK');}catch(Throwable $ignored){}$error=str_contains(strtolower($e->getMessage()),'unique')?'The next lot number is already in use. Reload and try again.':$e->getMessage();}
}
$pending=db()->query("SELECT r.id AS result_id,r.auction_id AS source_auction_id,r.pending_disposition,i.id AS item_id,i.lot_number,i.title,i.description,i.condition_notes,i.category,i.image_path,i.opening_price_cents,i.minimum_increment_cents,i.buyout_enabled,i.buyout_price_cents,a.title AS source_auction,a.results_status FROM auction_results r JOIN auction_items i ON i.id=r.auction_item_id JOIN auctions a ON a.id=r.auction_id WHERE r.result_status='unsold' AND r.pending_disposition='pending' AND a.results_status='released' ORDER BY r.finalized_at,a.title,CAST(i.lot_number AS INTEGER),i.lot_number")->fetchAll();
$drafts=db()->query("SELECT id,title FROM auctions WHERE status='draft' ORDER BY created_at DESC,id DESC")->fetchAll();
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Pending items','pending-items',$user);
?>
<section class="welcome"><div><p class="eyebrow">UNSOLD INVENTORY</p><h1>Pending Items</h1><p class="muted">Carry unsold items into a future draft auction without re-uploading details. Each new listing links back to the original item and keeps its prior bids/results.</p></div><div class="welcome-icon">↻</div></section>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?><?php if($success!==''): ?><div class="notice"><span class="notice-dot"></span><div><strong>Item relisted as a draft</strong><p><?= e($success) ?></p></div></div><?php endif; ?>
<?php if(!$drafts): ?><div class="notice"><span class="notice-dot"></span><div><strong>Create a draft auction first</strong><p>Items can only be copied into an auction that is still in Draft status.</p><a class="text-link" href="<?= e(app_url('admin/auctions.php')) ?>">Create auction draft →</a></div></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">AVAILABLE TO RE-AUCTION</p><h2>Unsold items</h2></div><span class="status-pill"><?= count($pending) ?> item(s)</span></div>
<?php if(!$pending): ?><div class="empty-state"><strong>No items are waiting in Pending Items</strong><p>During auction close-out, procurement can send unsold lots here for a future run.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><th>Original item</th><th>Original auction</th><th>Opening price</th><th>New draft auction</th></tr></thead><tbody><?php foreach($pending as $item): ?><tr><td><?php if($item['image_path']): ?><img class="table-thumb" src="<?= e(app_url('item-image.php?id='.(int)$item['item_id'])) ?>" alt=""><?php endif; ?><strong><?= e($item['title']) ?></strong><small class="table-sub">Original lot <?= e($item['lot_number']) ?> · <?= e($item['category']??'Uncategorised') ?></small><?php if($item['description']): ?><small class="table-sub"><?= e($item['description']) ?></small><?php endif; ?><?php if($item['condition_notes']): ?><small class="table-sub">Condition: <?= e($item['condition_notes']) ?></small><?php endif; ?></td><td><?= e($item['source_auction']) ?></td><td>N$ <?= number_format((int)$item['opening_price_cents']/100,2,'.',',') ?></td><td><?php if(!$drafts): ?><span class="muted">No draft auctions available</span><?php else: ?><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="result_id" value="<?= (int)$item['result_id'] ?>"><label for="auction-<?= (int)$item['result_id'] ?>">Destination</label><select id="auction-<?= (int)$item['result_id'] ?>" name="target_auction_id" required><?php foreach($drafts as $draft): ?><option value="<?= (int)$draft['id'] ?>"><?= e($draft['title']) ?></option><?php endforeach; ?></select><p class="form-help">New lot number will be assigned automatically for the destination auction.</p><button class="button button-primary" type="submit">Copy to draft →</button></form><?php endif; ?></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?></section>
<p><a class="text-link" href="<?= e(app_url('admin/closeout.php')) ?>">← Return to auction close-out</a></p>
<?php render_page_end(); ?>
