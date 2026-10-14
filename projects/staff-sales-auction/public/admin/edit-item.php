<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
$id=filter_input(INPUT_GET,'id',FILTER_VALIDATE_INT) ?: (int)($_POST['item_id']??0);
$q=db()->prepare('SELECT i.*,a.title AS auction_title,a.status AS auction_status FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.id=:id');$q->execute(['id'=>$id]);$item=$q->fetch();
if(!$item){http_response_code(404);exit('Auction item not found.');}
$error='';
function price_to_cents(string $value):?int{$value=trim(str_replace([' ', ','],['',''],$value));if($value===''||!preg_match('/^\\d{1,9}(?:\\.\\d{1,2})?$/',$value))return null;$parts=explode('.',$value,2);return ((int)$parts[0]*100)+(int)str_pad($parts[1]??'',2,'0');}
function cents_to_price(int $value):string{return number_format($value/100,2,'.','');}
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $lot=trim((string)($_POST['lot_number']??''));$title=trim((string)($_POST['title']??''));$description=trim((string)($_POST['description']??''));$condition=trim((string)($_POST['condition_notes']??''));$opening=price_to_cents((string)($_POST['opening_price']??''));$increment=price_to_cents((string)($_POST['minimum_increment']??''));$status=(string)($_POST['status']??'draft');$newImage=null;$oldImage=$item['image_path'];
 if($item['status']==='awarded')$error='Awarded items are locked against editing.';
 elseif($lot===''||mb_strlen($lot)>40)$error='Enter a lot number of 1–40 characters.';
 elseif($title===''||mb_strlen($title)>180)$error='Enter an item title of 1–180 characters.';
 elseif(mb_strlen($description)>10000||mb_strlen($condition)>3000)$error='The description or condition notes are too long.';
 elseif($opening===null)$error='Enter a valid opening price, for example 25.00.';
 elseif($increment===null||$increment<1)$error='Minimum increment must be greater than zero.';
 elseif(!in_array($status,['draft','published','withdrawn'],true))$error='Invalid item status.';
 else{
  $upload=$_FILES['item_image']??null;
  if(is_array($upload)&&($upload['error']??UPLOAD_ERR_NO_FILE)!==UPLOAD_ERR_NO_FILE){
   if(($upload['error']??UPLOAD_ERR_OK)!==UPLOAD_ERR_OK)$error='Image upload failed (code '.(int)$upload['error'].').';
   elseif((int)($upload['size']??0)>5*1024*1024)$error='Images must be 5 MB or smaller.';
   elseif(!is_uploaded_file((string)($upload['tmp_name']??'')))$error='The uploaded image could not be verified.';
   else{$tmp=(string)$upload['tmp_name'];$mime=(new finfo(FILEINFO_MIME_TYPE))->file($tmp);$imageInfo=@getimagesize($tmp);$ext=['image/jpeg'=>'jpg','image/png'=>'png','image/webp'=>'webp'][$mime]??null;
    if(!$ext||!$imageInfo)$error='Use a valid JPEG, PNG or WebP image.';
    elseif((int)$imageInfo[0]>6000||(int)$imageInfo[1]>6000)$error='Image dimensions must be 6000 × 6000 pixels or smaller.';
    else{$folder=dirname(__DIR__,2).'/storage/item-images';if(!is_dir($folder)&&!mkdir($folder,0770,true)&&!is_dir($folder))$error='Image storage folder could not be created.';elseif(!is_writable($folder))$error='Image storage folder is not writable by the web server.';else{$newImage=bin2hex(random_bytes(20)).'.'.$ext;if(!move_uploaded_file($tmp,$folder.'/'.$newImage)){$newImage=null;$error='Could not save the uploaded image.';}}}
   }
  }
  if($error===''){
   try{$imagePath=$newImage??$oldImage;$q=db()->prepare('UPDATE auction_items SET lot_number=:lot,title=:title,description=:description,condition_notes=:condition,opening_price_cents=:opening,minimum_increment_cents=:increment,status=:status,image_path=:image,updated_at=datetime(\'now\') WHERE id=:id');$q->execute(['lot'=>$lot,'title'=>$title,'description'=>$description!==''?$description:null,'condition'=>$condition!==''?$condition:null,'opening'=>$opening,'increment'=>$increment,'status'=>$status,'image'=>$imagePath,'id'=>$id]);audit_log((int)$user['id'],'auction_item.updated','auction_item',(string)$id,['auction_id'=>(int)$item['auction_id'],'status'=>$status,'image_updated'=>$newImage!==null]);if($newImage!==null&&$oldImage){$old=dirname(__DIR__,2).'/storage/item-images/'.basename((string)$oldImage);if(is_file($old))@unlink($old);}redirect_to('admin/edit-item.php?id='.$id.'&saved=1');}
   catch(PDOException $ex){if($newImage!==null){$f=dirname(__DIR__,2).'/storage/item-images/'.$newImage;if(is_file($f))@unlink($f);}if(str_contains(strtolower($ex->getMessage()),'unique'))$error='That lot number is already used in this auction.';else throw $ex;}
  }elseif($newImage!==null){$f=dirname(__DIR__,2).'/storage/item-images/'.$newImage;if(is_file($f))@unlink($f);}
 }
 $q=db()->prepare('SELECT i.*,a.title AS auction_title,a.status AS auction_status FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.id=:id');$q->execute(['id'=>$id]);$item=$q->fetch();
}
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Edit item','auctions',$user);
?>
<section class="welcome"><div><p class="eyebrow">AUCTION CATALOGUE · LOT <?= e($item['lot_number']) ?></p><h1>Edit item</h1><p class="muted"><?= e($item['auction_title']) ?> · Update item details, price, status or photo.</p></div><div class="welcome-icon">✎</div></section>
<?php if(isset($_GET['saved'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Item updated</strong><p>Your changes have been saved.</p></div></div><?php endif; ?><?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><form method="post" enctype="multipart/form-data" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="item_id" value="<?= (int)$item['id'] ?>"><div class="form-grid"><div><label for="lot_number">Lot number</label><input id="lot_number" name="lot_number" maxlength="40" required value="<?= e($item['lot_number']) ?>" <?= $item['status']==='awarded'?'disabled':'' ?>></div><div><label for="title">Item title</label><input id="title" name="title" maxlength="180" required value="<?= e($item['title']) ?>" <?= $item['status']==='awarded'?'disabled':'' ?>></div></div><label for="description">Description</label><textarea id="description" name="description" rows="4" maxlength="10000" <?= $item['status']==='awarded'?'disabled':'' ?>><?= e($item['description']??'') ?></textarea><label for="condition_notes">Condition notes</label><textarea id="condition_notes" name="condition_notes" rows="3" maxlength="3000" <?= $item['status']==='awarded'?'disabled':'' ?>><?= e($item['condition_notes']??'') ?></textarea><div class="form-grid"><div><label for="opening_price">Opening price (NAD)</label><input id="opening_price" name="opening_price" required inputmode="decimal" value="<?= e(cents_to_price((int)$item['opening_price_cents'])) ?>" <?= $item['status']==='awarded'?'disabled':'' ?>></div><div><label for="minimum_increment">Minimum bid increment (NAD)</label><input id="minimum_increment" name="minimum_increment" required inputmode="decimal" value="<?= e(cents_to_price((int)$item['minimum_increment_cents'])) ?>" <?= $item['status']==='awarded'?'disabled':'' ?>></div></div><label for="status">Item status</label><select id="status" name="status" <?= $item['status']==='awarded'?'disabled':'' ?>><?php foreach(['draft'=>'Draft — hidden from staff','published'=>'Published','withdrawn'=>'Withdrawn'] as $key=>$label): ?><option value="<?= e($key) ?>" <?= $item['status']===$key?'selected':'' ?>><?= e($label) ?></option><?php endforeach; ?><?php if($item['status']==='awarded'): ?><option value="awarded" selected>Awarded — locked</option><?php endif; ?></select><?php if($item['image_path']): ?><div class="image-preview"><img src="<?= e(app_url('item-image.php?id='.(int)$item['id'])) ?>" alt="<?= e($item['title']) ?>"><span>Current item image</span></div><?php endif; ?><label for="item_image">Item image <span class="muted">(optional)</span></label><input id="item_image" name="item_image" type="file" accept="image/jpeg,image/png,image/webp" <?= $item['status']==='awarded'?'disabled':'' ?>><p class="form-help">JPEG, PNG or WebP, maximum 5 MB and 6000 × 6000 pixels. Images are stored outside the public web root.</p><?php if($item['status']!=='awarded'): ?><button class="button button-primary" type="submit">Save changes →</button><?php else: ?><p class="form-help">Awarded items are locked. Contact your system administrator if a correction is required.</p><?php endif; ?></form></section><p><a class="text-link" href="<?= e(app_url('admin/items.php?auction='.(int)$item['auction_id'])) ?>">← Return to this auction's catalogue</a></p>
<?php render_page_end(); ?>