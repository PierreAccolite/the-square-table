<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_permission('procurement','custodian');
$error='';$success='';
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $action=(string)($_POST['action']??'');
 if($action==='add'){
  $name=trim((string)($_POST['name']??''));
  if($name===''||mb_strlen($name)>80)$error='Category name must be between 1 and 80 characters.';
  else{
   try{$q=db()->prepare("INSERT INTO item_categories(name,is_active) VALUES(:name,1) ON CONFLICT(name) DO UPDATE SET is_active=1");$q->execute(['name'=>$name]);audit_log((int)$user['id'],'item_category.created','item_category',$name,['name'=>$name]);redirect_to('categories.php?saved=1');}
   catch(PDOException $ex){$error='Could not save that category. Check whether the name is already in use.';}
  }
 }elseif($action==='deactivate'){
  $id=(int)($_POST['category_id']??0);$q=db()->prepare('SELECT id,name FROM item_categories WHERE id=:id');$q->execute(['id'=>$id]);$category=$q->fetch();
  if(!$category)$error='Category not found.';
  elseif(strcasecmp((string)$category['name'],'Uncategorised')===0)$error='The Uncategorised option cannot be deactivated.';
  else{$q=db()->prepare('SELECT COUNT(*) FROM auction_items WHERE category=:name');$q->execute(['name'=>$category['name']]);if((int)$q->fetchColumn()>0)$error='This category is assigned to existing items. Reassign those items before deactivating it.';else{$q=db()->prepare('UPDATE item_categories SET is_active=0 WHERE id=:id');$q->execute(['id'=>$id]);audit_log((int)$user['id'],'item_category.deactivated','item_category',(string)$id,['name'=>$category['name']]);redirect_to('categories.php?deactivated=1');}}
 }
}
$categories=db()->query("SELECT c.id,c.name,c.is_active,c.created_at,(SELECT COUNT(*) FROM auction_items i WHERE i.category=c.name) AS item_count FROM item_categories c ORDER BY c.is_active DESC,c.name COLLATE NOCASE")->fetchAll();
require dirname(__DIR__).'/app/layout.php';render_page_start('Item categories','categories',$user);
?>
<section class="welcome"><div><p class="eyebrow">ADMIN SETTINGS</p><h1>Item categories</h1><p class="muted">Maintain the categories available to procurement when creating or editing auction lots.</p></div><div class="welcome-icon">▦</div></section>
<?php if(isset($_GET['saved'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Category saved</strong></div></div><?php endif; ?><?php if(isset($_GET['deactivated'])): ?><div class="notice"><span class="notice-dot"></span><div><strong>Category deactivated</strong></div></div><?php endif; ?><?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">NEW CATEGORY</p><h2>Add a category</h2></div></div><form method="post" class="inline-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="add"><label for="name">Category name</label><input id="name" name="name" required maxlength="80" placeholder="e.g. Electrical spares"><button class="button button-primary" type="submit">Save category →</button></form><p class="form-help">Category names are shared by the catalogue and item administration pages. A category assigned to existing lots must be reassigned before it can be deactivated.</p></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">CATEGORY LIST</p><h2>Available categories</h2></div><span class="status-pill"><?= count(array_filter($categories,static fn($c)=>(int)$c['is_active']===1)) ?> active</span></div><div class="table-wrap"><table class="data-table"><thead><tr><th>Category</th><th>Assigned lots</th><th>Status</th><th>Action</th></tr></thead><tbody><?php foreach($categories as $category): ?><tr><td><strong><?= e($category['name']) ?></strong></td><td><?= (int)$category['item_count'] ?></td><td><span class="status-pill"><?= (int)$category['is_active']===1?'Active':'Inactive' ?></span></td><td><?php if((int)$category['is_active']===1&&(int)$category['item_count']===0&&strcasecmp((string)$category['name'],'Uncategorised')!==0): ?><form method="post" onsubmit="return confirm('Deactivate this category?');"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="deactivate"><input type="hidden" name="category_id" value="<?= (int)$category['id'] ?>"><button class="button button-quiet" type="submit">Deactivate</button></form><?php else: ?>—<?php endif; ?></td></tr><?php endforeach; ?></tbody></table></div></section>
<?php render_page_end(); ?>
