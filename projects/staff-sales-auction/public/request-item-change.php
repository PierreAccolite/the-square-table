<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_permission('procurement','custodian');
$id=filter_input(INPUT_GET,'id',FILTER_VALIDATE_INT) ?: (int)($_POST['item_id']??0);
$q=db()->prepare('SELECT i.*,a.title AS auction_title FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.id=:id');$q->execute(['id'=>$id]);$item=$q->fetch();
if(!$item){http_response_code(404);exit('Auction item not found.');}
$error='';
function change_price_cents(string $v):?int{$v=trim(str_replace([' ', ','],['',''],$v));if($v===''||!preg_match('/^\d{1,9}(?:\.\d{1,2})?$/',$v))return null;$p=explode('.',$v,2);return (int)$p[0]*100+(int)str_pad($p[1]??'',2,'0');}
function change_cents_price(int $v):string{return number_format($v/100,2,'.','');}
$keys=['lot_number','title','description','condition_notes','opening_price_cents','minimum_increment_cents','status','buyout_enabled','buyout_price_cents'];
$before=[];foreach($keys as $k)$before[$k]=$item[$k]??null;
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $lot=trim((string)($_POST['lot_number']??''));$title=trim((string)($_POST['title']??''));$description=trim((string)($_POST['description']??''));$condition=trim((string)($_POST['condition_notes']??''));$opening=change_price_cents((string)($_POST['opening_price']??''));$increment=change_price_cents((string)($_POST['minimum_increment']??''));$status=(string)($_POST['status']??'draft');$buyoutEnabled=isset($_POST['buyout_enabled'])?1:0;$buyoutPrice=change_price_cents((string)($_POST['buyout_price']??''));$reason=trim((string)($_POST['reason']??''));
 if($item['status']==='awarded')$error='Awarded items cannot be changed.';
 elseif($lot===''||mb_strlen($lot)>40)$error='Lot number must be 1–40 characters.';
 elseif($title===''||mb_strlen($title)>180)$error='Item title must be 1–180 characters.';
 elseif(mb_strlen($description)>10000||mb_strlen($condition)>3000)$error='Description or condition notes are too long.';
 elseif($opening===null)$error='Enter a valid opening price.';
 elseif($increment===null||$increment<1)$error='Minimum increment must be greater than zero.';
 elseif(!in_array($status,['draft','published','withdrawn'],true))$error='Invalid requested item status.';
 elseif($buyoutEnabled===1&&($buyoutPrice===null||$buyoutPrice<1))$error='Enter a buyout price greater than zero, or turn buyout off.';
 elseif(mb_strlen($reason)<5||mb_strlen($reason)>2000)$error='Explain the business reason for this change (5–2,000 characters).';
 else{
  $pending=db()->prepare("SELECT COUNT(*) FROM approval_requests WHERE entity_type='auction_item' AND entity_id=:id AND status='pending'");$pending->execute(['id'=>$id]);
  if((int)$pending->fetchColumn()>0)$error='There is already a pending change request for this item. Wait for it to be reviewed.';
  else{
   $after=['lot_number'=>$lot,'title'=>$title,'description'=>$description!==''?$description:null,'condition_notes'=>$condition!==''?$condition:null,'opening_price_cents'=>$opening,'minimum_increment_cents'=>$increment,'status'=>$status,'buyout_enabled'=>$buyoutEnabled,'buyout_price_cents'=>$buyoutEnabled?$buyoutPrice:null];
   if($after===$before)$error='No changes were made to the item.';
   else{$ins=db()->prepare("INSERT INTO approval_requests(request_type,entity_type,entity_id,requested_by,reason,before_json,after_json) VALUES('item_change','auction_item',:id,:actor,:reason,:before,:after)");$ins->execute(['id'=>$id,'actor'=>(int)$user['id'],'reason'=>$reason,'before'=>json_encode($before,JSON_THROW_ON_ERROR|JSON_UNESCAPED_UNICODE),'after'=>json_encode($after,JSON_THROW_ON_ERROR|JSON_UNESCAPED_UNICODE)]);$rid=(int)db()->lastInsertId();audit_log((int)$user['id'],'approval.requested','auction_item',(string)$id,['request_id'=>$rid]);redirect_to('request-item-change.php?id='.$id.'&submitted=1');}
  }
 }
}
require dirname(__DIR__).'/app/layout.php';render_page_start('Request item change','catalogue',$user);
?>
<section class="welcome"><div><p class="eyebrow">PROCUREMENT WORKFLOW · LOT <?= e($item['lot_number']) ?></p><h1>Request item change</h1><p class="muted"><?= e($item['auction_title']) ?> · Changes are reviewed before they are applied.</p></div><div class="welcome-icon">⇄</div></section>
<?php if(isset($_GET['submitted'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Change request submitted</strong><p>The current item remains unchanged until an authorised approver reviews the request.</p></div></div><?php endif; ?><?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<?php if($item['status']==='awarded'): ?><section class="content-panel empty-state"><strong>This item is locked</strong><p>Awarded items cannot be changed.</p></section><?php else: ?>
<section class="content-panel"><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="item_id" value="<?= (int)$id ?>"><div class="form-grid"><div><label for="lot_number">Lot number</label><input id="lot_number" name="lot_number" maxlength="40" required value="<?= e($item['lot_number']) ?>"></div><div><label for="title">Item title</label><input id="title" name="title" maxlength="180" required value="<?= e($item['title']) ?>"></div></div><label for="description">Description</label><textarea id="description" name="description" rows="4" maxlength="10000"><?= e($item['description']??'') ?></textarea><label for="condition_notes">Condition notes</label><textarea id="condition_notes" name="condition_notes" rows="3" maxlength="3000"><?= e($item['condition_notes']??'') ?></textarea><div class="form-grid"><div><label for="opening_price">Opening price (NAD)</label><input id="opening_price" name="opening_price" inputmode="decimal" required value="<?= e(change_cents_price((int)$item['opening_price_cents'])) ?>"></div><div><label for="minimum_increment">Minimum bid increment (NAD)</label><input id="minimum_increment" name="minimum_increment" inputmode="decimal" required value="<?= e(change_cents_price((int)$item['minimum_increment_cents'])) ?>"></div></div><div class="form-grid"><div><label><input type="checkbox" name="buyout_enabled" value="1" <?= (int)($item['buyout_enabled']??0)===1?'checked':'' ?>> Allow buyout</label><p class="form-help">Setting only; purchase processing is not active.</p></div><div><label for="buyout_price">Buyout price (NAD)</label><input id="buyout_price" name="buyout_price" inputmode="decimal" value="<?= e(change_cents_price((int)($item['buyout_price_cents']??0))) ?>"></div></div><label for="status">Requested status</label><select id="status" name="status"><?php foreach(['draft'=>'Draft','published'=>'Published','withdrawn'=>'Withdrawn'] as $key=>$label): ?><option value="<?= e($key) ?>" <?= $item['status']===$key?'selected':'' ?>><?= e($label) ?></option><?php endforeach; ?></select><label for="reason">Reason for this change</label><textarea id="reason" name="reason" rows="3" maxlength="2000" required placeholder="Explain why the change is needed and who requested it."></textarea><p class="form-help">A different authorised person must approve or reject this request. Item photos are not included in this approval form.</p><button class="button button-primary" type="submit">Submit for approval →</button></form></section><?php endif; ?>
<p><a class="text-link" href="<?= e(app_url('auctions.php')) ?>">← Return to staff catalogue</a></p><?php render_page_end(); ?>