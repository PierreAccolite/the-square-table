<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_permission('procurement','custodian','approver');
$canReviewProcurement=has_permission($user,'procurement')||has_permission($user,'custodian');
$canReviewManager=has_permission($user,'approver');
$error='';
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $rid=(int)($_POST['request_id']??0);$decision=(string)($_POST['decision']??'');$comment=trim((string)($_POST['review_comment']??''));
 $pdo=db();$q=$pdo->prepare("SELECT * FROM approval_requests WHERE id=:id AND status='pending'");$q->execute(['id'=>$rid]);$req=$q->fetch();
 if(!$req)$error='This request is no longer pending.';
 elseif((int)$req['requested_by']===(int)$user['id'])$error='You cannot review your own change request.';
 elseif(!in_array($decision,['approve','reject'],true))$error='Choose approve or reject.';
 elseif(mb_strlen($comment)>2000)$error='Review comment is too long.';
 elseif(($req['approval_stage']??'procurement')==='procurement'&&!$canReviewProcurement)$error='This request is waiting for procurement/custodian review.';
 elseif(($req['approval_stage']??'procurement')==='manager'&&!$canReviewManager)$error='This request is waiting for line-management approval.';
 elseif(($req['approval_stage']??'procurement')==='manager'&&(int)($req['procurement_reviewed_by']??0)===(int)$user['id'])$error='The procurement reviewer cannot provide the final line-management approval.';
 else{
  try{
   $pdo->beginTransaction();
   $stage=(string)($req['approval_stage']??'procurement');
   if($decision==='reject'){
    $q=$pdo->prepare("UPDATE approval_requests SET status='rejected',reviewed_by=:reviewer,review_comment=:comment,reviewed_at=datetime('now') WHERE id=:id AND status='pending' AND approval_stage=:stage");
    $q->execute(['reviewer'=>(int)$user['id'],'comment'=>$comment!==''?$comment:null,'id'=>$rid,'stage'=>$stage]);
    if($q->rowCount()!==1)throw new RuntimeException('The request has already been reviewed.');
    if($stage==='procurement'){$q=$pdo->prepare("UPDATE approval_requests SET procurement_reviewed_by=:reviewer,procurement_reviewed_at=datetime('now') WHERE id=:id");$q->execute(['reviewer'=>(int)$user['id'],'id'=>$rid]);}
    $pdo->commit();audit_log((int)$user['id'],$stage==='procurement'?'approval.procurement_rejected':'approval.manager_rejected',$req['entity_type'],(string)$req['entity_id'],['request_id'=>$rid,'comment'=>$comment]);redirect_to('admin/approvals.php?rejected=1');
   }
   if($stage==='procurement'){
    $q=$pdo->prepare("UPDATE approval_requests SET approval_stage='manager',procurement_reviewed_by=:reviewer,procurement_reviewed_at=datetime('now') WHERE id=:id AND status='pending' AND approval_stage='procurement'");
    $q->execute(['reviewer'=>(int)$user['id'],'id'=>$rid]);
    if($q->rowCount()!==1)throw new RuntimeException('The request has already moved to another stage.');
    $pdo->commit();audit_log((int)$user['id'],'approval.procurement_approved',$req['entity_type'],(string)$req['entity_id'],['request_id'=>$rid,'comment'=>$comment]);redirect_to('admin/approvals.php?manager_review=1');
   }
   $before=json_decode((string)$req['before_json'],true,512,JSON_THROW_ON_ERROR);
   $after=json_decode((string)$req['after_json'],true,512,JSON_THROW_ON_ERROR);
   $q=$pdo->prepare('SELECT * FROM auction_items WHERE id=:id');$q->execute(['id'=>(int)$req['entity_id']]);$item=$q->fetch();
   if(!$item)throw new RuntimeException('The item no longer exists.');
   $keys=['lot_number','title','description','condition_notes','opening_price_cents','minimum_increment_cents','status','buyout_enabled','buyout_price_cents'];$current=[];foreach($keys as $key)$current[$key]=$item[$key]??null;
   if($current!==$before){
    $q=$pdo->prepare("UPDATE approval_requests SET status='stale',reviewed_by=:reviewer,review_comment='Item changed after this request was submitted; submit a new request.',reviewed_at=datetime('now') WHERE id=:id AND status='pending' AND approval_stage='manager'");
    $q->execute(['reviewer'=>(int)$user['id'],'id'=>$rid]);$pdo->commit();audit_log((int)$user['id'],'approval.stale',$req['entity_type'],(string)$req['entity_id'],['request_id'=>$rid]);redirect_to('admin/approvals.php?stale=1');
   }
   $q=$pdo->prepare("UPDATE auction_items SET lot_number=:lot,title=:title,description=:description,condition_notes=:condition,opening_price_cents=:opening,minimum_increment_cents=:increment,status=:status,buyout_enabled=:buyout,buyout_price_cents=:price,updated_at=datetime('now') WHERE id=:id");
   $q->execute(['lot'=>$after['lot_number'],'title'=>$after['title'],'description'=>$after['description'],'condition'=>$after['condition_notes'],'opening'=>$after['opening_price_cents'],'increment'=>$after['minimum_increment_cents'],'status'=>$after['status'],'buyout'=>$after['buyout_enabled'],'price'=>$after['buyout_price_cents'],'id'=>(int)$req['entity_id']]);
   $q=$pdo->prepare("UPDATE approval_requests SET status='approved',reviewed_by=:reviewer,review_comment=:comment,reviewed_at=datetime('now') WHERE id=:id AND status='pending' AND approval_stage='manager'");
   $q->execute(['reviewer'=>(int)$user['id'],'comment'=>$comment!==''?$comment:null,'id'=>$rid]);
   if($q->rowCount()!==1)throw new RuntimeException('The request has already been reviewed.');
   $pdo->commit();audit_log((int)$user['id'],'approval.manager_approved',$req['entity_type'],(string)$req['entity_id'],['request_id'=>$rid,'comment'=>$comment]);audit_log((int)$user['id'],'auction_item.updated_by_approval','auction_item',(string)$req['entity_id'],['request_id'=>$rid]);redirect_to('admin/approvals.php?approved=1');
  }catch(Throwable $ex){if($pdo->inTransaction())$pdo->rollBack();if($ex instanceof PDOException&&str_contains(strtolower($ex->getMessage()),'unique'))$error='Approval would duplicate a lot number. The item was not changed.';elseif($ex instanceof RuntimeException)$error=$ex->getMessage();else throw $ex;}
 }
}
$requests=db()->query("SELECT r.*,u.display_name AS requester_name,u.username AS requester_username,i.title AS item_title,i.lot_number,a.title AS auction_title,p.display_name AS procurement_reviewer_name,m.display_name AS manager_reviewer_name FROM approval_requests r JOIN users u ON u.id=r.requested_by LEFT JOIN auction_items i ON i.id=r.entity_id LEFT JOIN auctions a ON a.id=i.auction_id LEFT JOIN users p ON p.id=r.procurement_reviewed_by LEFT JOIN users m ON m.id=r.reviewed_by ORDER BY CASE WHEN r.status='pending' AND r.approval_stage='procurement' THEN 0 WHEN r.status='pending' AND r.approval_stage='manager' THEN 1 ELSE 2 END,r.created_at DESC LIMIT 100")->fetchAll();
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Approvals','approvals',$user);
?>
<section class="welcome"><div><p class="eyebrow">PROCUREMENT GOVERNANCE</p><h1>Change approvals</h1><p class="muted">Item changes pass through procurement/custodian review, followed by a separate line-management decision. The item remains unchanged until final approval.</p></div><div class="welcome-icon">✓</div></section>
<?php foreach(['approved'=>'Both reviews are complete; the item change has been applied.','rejected'=>'Change request rejected.','stale'=>'Request marked stale because the item changed after submission.','manager_review'=>'Procurement review is complete; the request is waiting for line management.'] as $key=>$msg): ?><?php if(isset($_GET[$key])): ?><div class="notice" role="status"><span class="notice-dot"></span><div><strong><?= e($msg) ?></strong></div></div><?php endif; ?><?php endforeach; ?>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">TWO-STAGE REVIEW</p><h2>How approvals work</h2></div></div><ol><li>Procurement or custodian reviews the business case and proposed changes.</li><li>A different authorised line manager/approver makes the final decision.</li><li>The item is updated only after the final approval. Stale requests must be resubmitted.</li></ol></section>
<?php if(!$requests): ?><section class="content-panel empty-state"><strong>No approval requests yet</strong><p>Procurement or custodian users can submit item changes for review.</p></section><?php else: ?>
<?php foreach($requests as $r): $after=json_decode((string)$r['after_json'],true)?:[];$before=json_decode((string)$r['before_json'],true)?:[];$stage=(string)($r['approval_stage']??'procurement');$isPending=$r['status']==='pending';$canAct=$isPending&&((($stage==='procurement')&&$canReviewProcurement)||(($stage==='manager')&&$canReviewManager&&(int)($r['procurement_reviewed_by']??0)!==(int)$user['id']))&&(int)$r['requested_by']!==(int)$user['id']; ?>
<section class="content-panel approval-card"><div class="section-heading"><div><p class="eyebrow">REQUEST #<?= (int)$r['id'] ?> · <?= e(strtoupper((string)$r['status'])) ?></p><h2><?= e($r['item_title']??'Item unavailable') ?> · Lot <?= e($r['lot_number']??'—') ?></h2><p class="muted"><?= e($r['auction_title']??'Auction unavailable') ?> · Requested by <?= e($r['requester_name']) ?> (<?= e($r['requester_username']) ?>) · <?= e($r['created_at']) ?></p></div><span class="status-pill"><?= $isPending?($stage==='procurement'?'Procurement review':'Line manager review'):e(ucfirst((string)$r['status'])) ?></span></div>
<p><strong>Reason</strong><br><?= nl2br(e($r['reason'])) ?></p>
<div class="table-wrap"><table class="data-table"><thead><tr><th>Field</th><th>Original value</th><th>Requested value</th></tr></thead><tbody><?php foreach(['lot_number'=>'Lot number','title'=>'Title','description'=>'Description','condition_notes'=>'Condition notes','opening_price_cents'=>'Opening price (NAD)','minimum_increment_cents'=>'Minimum increment (NAD)','status'=>'Status','buyout_enabled'=>'Buyout enabled','buyout_price_cents'=>'Buyout price (NAD)'] as $key=>$label): ?><?php if(($before[$key]??null)!==($after[$key]??null)): ?><tr><td><?= e($label) ?></td><td><?php if(str_ends_with($key,'_cents')): ?>N$ <?= e(number_format(((int)($before[$key]??0))/100,2)) ?><?php elseif($key==='buyout_enabled'): ?><?= (int)($before[$key]??0)?'Yes':'No' ?><?php else: ?><?= e($before[$key]??'—') ?><?php endif; ?></td><td><?php if(str_ends_with($key,'_cents')): ?>N$ <?= e(number_format(((int)($after[$key]??0))/100,2)) ?><?php elseif($key==='buyout_enabled'): ?><?= (int)($after[$key]??0)?'Yes':'No' ?><?php else: ?><?= e($after[$key]??'—') ?><?php endif; ?></td></tr><?php endif; ?><?php endforeach; ?></tbody></table></div>
<?php if($r['procurement_reviewed_at']): ?><p class="form-help">Procurement/custodian review: <?= e($r['procurement_reviewer_name']??'Reviewer') ?> · <?= e($r['procurement_reviewed_at']) ?></p><?php endif; ?>
<?php if($isPending&&$canAct): ?><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="request_id" value="<?= (int)$r['id'] ?>"><label for="comment<?= (int)$r['id'] ?>">Review note <span class="muted">(optional)</span></label><textarea id="comment<?= (int)$r['id'] ?>" name="review_comment" rows="2" maxlength="2000"></textarea><div class="dashboard-actions"><button class="button button-primary" name="decision" value="approve" type="submit" onclick="return confirm('<?= $stage==='procurement'?'Send this request to line management for final approval?':'Approve and apply this item change?' ?>');"><?= $stage==='procurement'?'Approve procurement review →':'Final approval and apply →' ?></button><button class="button button-quiet" name="decision" value="reject" type="submit" onclick="return confirm('Reject this item change request?');">Reject request</button></div></form><?php elseif($isPending): ?><p class="form-help"><?= (int)$r['requested_by']===(int)$user['id']?'You submitted this request; another user must review it.':($stage==='manager'&&(int)($r['procurement_reviewed_by']??0)===(int)$user['id']?'You completed the first review; a different line manager must make the final decision.':($stage==='procurement'?'Waiting for procurement/custodian review.':'Waiting for line-management approval.')) ?></p><?php else: ?><p class="form-help">Completed <?= e($r['reviewed_at']??'') ?> · Final reviewer: <?= e($r['manager_reviewer_name']??'Not recorded') ?> · <?= e($r['review_comment']??'No review note') ?></p><?php endif; ?></section>
<?php endforeach; ?><?php endif; ?>
<?php render_page_end(); ?>