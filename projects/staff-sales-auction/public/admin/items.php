<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
function money_to_cents(string $value):?int{
 $value=trim(str_replace([' ', ','],['',''],$value));
 if($value===''||!preg_match('/^\\d{1,9}(?:\\.\\d{1,2})?$/',$value))return null;
 $parts=explode('.',$value,2);$fraction=str_pad($parts[1]??'',2,'0');
 return ((int)$parts[0]*100)+(int)$fraction;
}
function cents_to_money(int $cents):string{return number_format($cents/100,2,'.','');}
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
$categories=db()->query('SELECT name FROM item_categories WHERE is_active=1 ORDER BY name COLLATE NOCASE')->fetchAll(PDO::FETCH_COLUMN);
$auctions=db()->query('SELECT id,title,status FROM auctions ORDER BY created_at DESC,id DESC')->fetchAll();
$selectedId=filter_input(INPUT_GET,'auction',FILTER_VALIDATE_INT) ?: (int)($auctions[0]['id']??0);
$selected=null;foreach($auctions as $a){if((int)$a['id']===$selectedId){$selected=$a;break;}}
$error='';$success='';
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token. Reload and try again.');}
 $selectedId=(int)($_POST['auction_id']??0);$selected=null;foreach($auctions as $a){if((int)$a['id']===$selectedId){$selected=$a;break;}}
 $action=(string)($_POST['action']??'create_item');
 if(!$selected){$error='Choose a valid auction.';}
 elseif($action==='copy_pending'){
  if($selected['status']!=='draft')$error='Pending items can only be copied into a draft auction.';
  else{
   $pdo=db();$pdo->exec('BEGIN IMMEDIATE');
   try{
    $q=$pdo->prepare("SELECT r.id AS result_id,r.auction_id,r.pending_disposition,r.result_status,i.*,a.results_status AS source_results_status FROM auction_results r JOIN auction_items i ON i.id=r.auction_item_id JOIN auctions a ON a.id=r.auction_id WHERE r.id=:id");$q->execute(['id'=>(int)($_POST['pending_result_id']??0)]);$source=$q->fetch();
    if(!$source||$source['result_status']!=='unsold'||$source['pending_disposition']!=='pending'||$source['source_results_status']!=='released')throw new RuntimeException('This item is no longer available in Pending Items.');
    $q=$pdo->prepare("SELECT id,title FROM auctions WHERE id=:id AND status='draft'");$q->execute(['id'=>$selectedId]);$target=$q->fetch();
    if(!$target)throw new RuntimeException('The selected auction is no longer a draft.');
    $lot=next_lot_number($pdo,$selectedId);$now=date('Y-m-d H:i:s');
    $q=$pdo->prepare("INSERT INTO auction_items(auction_id,lot_number,title,description,condition_notes,image_path,opening_price_cents,minimum_increment_cents,status,created_at,updated_at,category,buyout_enabled,buyout_price_cents,source_item_id) VALUES(:auction,:lot,:title,:description,:condition,:image,:opening,:increment,'draft',:now,:now,:category,:buyout_enabled,:buyout_price,:source)");
    $q->execute(['auction'=>$selectedId,'lot'=>$lot,'title'=>$source['title'],'description'=>$source['description'],'condition'=>$source['condition_notes'],'image'=>$source['image_path'],'opening'=>$source['opening_price_cents'],'increment'=>$source['minimum_increment_cents'],'now'=>$now,'category'=>$source['category']??'Uncategorised','buyout_enabled'=>$source['buyout_enabled']??0,'buyout_price'=>$source['buyout_price_cents'],'source'=>$source['id']]);
    $newItemId=(int)$pdo->lastInsertId();
    $q=$pdo->prepare("UPDATE auction_results SET pending_disposition='relisted',relisted_item_id=:new_item WHERE id=:id AND pending_disposition='pending'");$q->execute(['new_item'=>$newItemId,'id'=>(int)$source['result_id']]);
    if($q->rowCount()!==1)throw new RuntimeException('Pending-item state changed. Reload and try again.');
    audit_log((int)$user['id'],'auction.unsold_item_relisted','auction_item',(string)$newItemId,['source_item_id'=>(int)$source['id'],'source_result_id'=>(int)$source['result_id'],'source_auction_id'=>(int)$source['auction_id'],'target_auction_id'=>$selectedId,'new_lot_number'=>$lot]);
    $pdo->commit();redirect_to('admin/items.php?auction='.$selectedId.'&relisted=1');
   }catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();if($ex instanceof PDOException&&str_contains(strtolower($ex->getMessage()),'unique'))$error='The next lot number is already in use. Reload and try again.';elseif($ex instanceof RuntimeException)$error=$ex->getMessage();else throw $ex;}
  }
 }elseif($action==='create_item'){
  $title=trim((string)($_POST['title']??''));$description=trim((string)($_POST['description']??''));$condition=trim((string)($_POST['condition_notes']??''));$category=trim((string)($_POST['category']??'Uncategorised'));
  $opening=money_to_cents((string)($_POST['opening_price']??''));$increment=money_to_cents((string)($_POST['minimum_increment']??'10.00'));$status=(string)($_POST['status']??'draft');$buyoutEnabled=isset($_POST['buyout_enabled'])?1:0;$buyoutPrice=money_to_cents((string)($_POST['buyout_price']??''));
  if($title===''||mb_strlen($title)>180)$error='Enter an item title of 1–180 characters.';
  elseif(mb_strlen($description)>10000||mb_strlen($condition)>3000)$error='The description or condition notes are too long.';
  elseif(!in_array($category,$categories,true))$error='Choose a valid item category.';
  elseif($opening===null)$error='Enter a valid opening price, for example 25.00.';
  elseif($increment===null||$increment<1)$error='Enter a minimum bid increment greater than zero.';
  elseif(!in_array($status,['draft','published'],true))$error='Invalid item status.';
  elseif($buyoutEnabled===1&&($buyoutPrice===null||$buyoutPrice<1))$error='Enter a buyout price greater than zero, or turn buyout off.';
  else{
   $pdo=db();$pdo->exec('BEGIN IMMEDIATE');
   try{
    $lot=next_lot_number($pdo,$selectedId);
    $q=$pdo->prepare('INSERT INTO auction_items(auction_id,lot_number,title,description,condition_notes,category,opening_price_cents,minimum_increment_cents,status,buyout_enabled,buyout_price_cents) VALUES(:auction,:lot,:title,:description,:condition,:category,:opening,:increment,:status,:buyout_enabled,:buyout_price)');
    $q->execute(['auction'=>$selectedId,'lot'=>$lot,'title'=>$title,'description'=>$description!==''?$description:null,'condition'=>$condition!==''?$condition:null,'category'=>$category,'opening'=>$opening,'increment'=>$increment,'status'=>$status,'buyout_enabled'=>$buyoutEnabled,'buyout_price'=>$buyoutEnabled?$buyoutPrice:null]);
    $id=(int)$pdo->lastInsertId();$pdo->commit();
    audit_log((int)$user['id'],'auction_item.created','auction_item',(string)$id,['auction_id'=>$selectedId,'lot_number'=>$lot,'title'=>$title,'category'=>$category,'status'=>$status]);
    redirect_to('admin/items.php?auction='.$selectedId.'&saved=1');
   }catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();if($ex instanceof PDOException&&str_contains(strtolower($ex->getMessage()),'unique'))$error='The next lot number is already in use. Reload and try again.';else throw $ex;}
  }
 }else{$error='Invalid item action.';}
}
$items=[];
if($selected){$q=db()->prepare('SELECT i.id,i.source_item_id,i.lot_number,i.title,i.description,i.condition_notes,i.category,i.image_path,i.opening_price_cents,i.minimum_increment_cents,i.buyout_enabled,i.buyout_price_cents,i.status,i.created_at,(SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count FROM auction_items i WHERE i.auction_id=:id ORDER BY CAST(lot_number AS INTEGER),lot_number,id');$q->execute(['id'=>$selectedId]);$items=$q->fetchAll();}
$nextLotNumber=$selected?next_lot_number(db(),$selectedId):'0001';
$pendingItems=db()->query("SELECT r.id AS result_id,i.title,i.lot_number,a.title AS source_auction FROM auction_results r JOIN auction_items i ON i.id=r.auction_item_id JOIN auctions a ON a.id=r.auction_id WHERE r.result_status='unsold' AND r.pending_disposition='pending' AND a.results_status='released' ORDER BY r.finalized_at,a.title,CAST(i.lot_number AS INTEGER),i.lot_number")->fetchAll();
?>
<?php require dirname(__DIR__,2).'/app/layout.php'; render_page_start('Item catalogue','items',$user); ?><section class="welcome"><div><p class="eyebrow">AUCTION CATALOGUE</p><h1>Manage items</h1><p class="muted">Add lots, categories, descriptions, condition notes and starting prices.</p></div><div class="welcome-icon">▤</div></section>
<?php if(isset($_GET['saved'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Item saved</strong><p>The item has been added to the selected auction.</p></div></div><?php endif; ?><?php if(isset($_GET['relisted'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Pending item added</strong><p>The item was copied into this draft auction with a new automatic lot number. The original record and bidding history remain unchanged.</p></div></div><?php endif; ?><?php if(isset($_GET['created'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Auction created</strong><p>Start by adding the first item below.</p></div></div><?php endif; ?><?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<?php if(!$auctions): ?><section class="content-panel empty-state"><strong>Create an auction first</strong><p>Items belong to an auction. Create a draft before adding catalogue items.</p><a class="button button-primary" href="<?= e(app_url('admin/auctions.php')) ?>">Create an auction →</a></section><?php else: ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">SELECT AUCTION</p><h2><?= e($selected['title']??'Choose an auction') ?></h2></div><span class="status-pill"><?= e(ucfirst((string)($selected['status']??''))) ?></span></div><form method="get" class="inline-form"><label for="auction">Auction</label><select id="auction" name="auction"><?php foreach($auctions as $a): ?><option value="<?= (int)$a['id'] ?>" <?= (int)$a['id']===$selectedId?'selected':'' ?>><?= e($a['title']) ?> · <?= e($a['status']) ?></option><?php endforeach; ?></select><button class="button button-quiet" type="submit">Open catalogue</button></form></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">NEW LOT</p><h2>Add an item</h2></div></div><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="create_item"><input type="hidden" name="auction_id" value="<?= (int)$selectedId ?>"><div class="form-grid"><div><label for="lot_number">Next lot number (automatic)</label><input id="lot_number" value="<?= e($nextLotNumber) ?>" readonly aria-readonly="true"><p class="form-help">Assigned by the server when this item is saved.</p></div><div><label for="title">Item title</label><input id="title" name="title" required maxlength="180" placeholder="e.g. Office desk"></div></div><div><label for="category">Category</label><select id="category" name="category" required><?php foreach($categories as $categoryOption): ?><option value="<?= e($categoryOption) ?>"><?= e($categoryOption) ?></option><?php endforeach; ?></select></div><label for="description">Description</label><textarea id="description" name="description" rows="3" maxlength="10000" placeholder="Brand, dimensions, accessories and other details"></textarea><label for="condition_notes">Condition notes</label><textarea id="condition_notes" name="condition_notes" rows="2" maxlength="3000" placeholder="Used condition, visible wear, faults or collection notes"></textarea><div class="form-grid"><div><label for="opening_price">Opening price (NAD)</label><input id="opening_price" name="opening_price" inputmode="decimal" required placeholder="0.00"></div><div><label for="minimum_increment">Minimum bid increment (NAD)</label><input id="minimum_increment" name="minimum_increment" inputmode="decimal" value="10.00" required></div></div><div class="form-grid"><div><label><input type="checkbox" name="buyout_enabled" value="1"> Allow buyout (purchase-now option)</label><p class="form-help">This records the option only; purchase processing is not active.</p></div><div><label for="buyout_price">Buyout price (NAD)</label><input id="buyout_price" name="buyout_price" inputmode="decimal" placeholder="0.00"></div></div><label for="status">Catalogue status</label><select id="status" name="status"><option value="draft">Draft — hidden from staff</option><option value="published">Published — ready for catalogue when auction is released</option></select><p class="form-help">Prices are stored as whole cents to avoid rounding errors.</p><button class="button button-primary" type="submit">Save item →</button></form></section>
<?php if($selected&&$selected['status']==='draft'): ?><section class="content-panel"><div class="section-heading"><div><p class="eyebrow">RELIST UNSOLD INVENTORY</p><h2>Select from Pending Items</h2><p class="muted">Copy an unsold item directly into this draft auction. A new lot number is assigned automatically.</p></div></div><?php if(!$pendingItems): ?><div class="empty-state"><strong>No pending items available</strong><p>Released unsold items marked Pending will appear here.</p></div><?php else: ?><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="copy_pending"><input type="hidden" name="auction_id" value="<?= (int)$selectedId ?>"><label for="pending_result_id">Pending item</label><select id="pending_result_id" name="pending_result_id" required><option value="">Choose an item…</option><?php foreach($pendingItems as $pendingItem): ?><option value="<?= (int)$pendingItem['result_id'] ?>"><?= e($pendingItem['title']) ?> · Original lot <?= e($pendingItem['lot_number']) ?> · <?= e($pendingItem['source_auction']) ?></option><?php endforeach; ?></select><button class="button button-primary" type="submit" onclick="return confirm('Copy this pending item into the selected draft auction?');">Add pending item to this auction →</button></form><?php endif; ?></section><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">LOT LIST</p><h2>Items in this auction</h2></div><span class="status-pill"><?= count($items) ?> items</span></div><?php if(!$items): ?><div class="empty-state"><strong>No items in this auction yet</strong><p>Add your first lot using the form above.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><th>Lot</th><th>Item</th><th>Category</th><th>Opening price</th><th>Increment</th><th>Bids</th><th>Buyout</th><th>Status</th><th>Actions</th></tr></thead><tbody><?php foreach($items as $item): ?><tr><td><?= e($item['lot_number']) ?></td><td><?php if($item['image_path']): ?><img class="table-thumb" src="<?= e(app_url('item-image.php?id='.(int)$item['id'])) ?>" alt=""><?php endif; ?><strong><?= e($item['title']) ?></strong><?php if($item['source_item_id']): ?><small class="table-sub">Re-listed from original item #<?= (int)$item['source_item_id'] ?></small><?php endif; ?><?php if($item['description']): ?><small class="table-sub"><?= e($item['description']) ?></small><?php endif; ?><?php if($item['condition_notes']): ?><small class="table-sub">Condition: <?= e($item['condition_notes']) ?></small><?php endif; ?></td><td><?= e($item['category']??'Uncategorised') ?></td><td>N$ <?= e(cents_to_money((int)$item['opening_price_cents'])) ?></td><td>N$ <?= e(cents_to_money((int)$item['minimum_increment_cents'])) ?></td><td><?= (int)$item['bid_count'] ?></td><td><?php if((int)$item['buyout_enabled']===1): ?>Buyout set<small class="table-sub">N$ <?= e(cents_to_money((int)$item['buyout_price_cents'])) ?></small><?php else: ?>—<?php endif; ?></td><td><span class="status-pill"><?= e(ucfirst($item['status'])) ?></span></td><td><a class="text-link" href="<?= e(app_url('admin/edit-item.php?id='.(int)$item['id'])) ?>">Edit item →</a></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?></section><?php endif; ?>
<p><a class="text-link" href="<?= e(app_url('admin/auctions.php')) ?>">← Return to auctions</a></p><?php render_page_end(); ?>
