<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
require dirname(__DIR__,2).'/app/auction_engine.php';
$user=require_permission('procurement','custodian','approver');
$canProcure=has_permission($user,'procurement')||has_permission($user,'custodian');
$canApprove=has_permission($user,'approver');
$error='';$success='';
$auctionId=(int)($_GET['auction']??$_POST['auction_id']??0);
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $action=(string)($_POST['action']??'');$auctionId=(int)($_POST['auction_id']??0);$pdo=db();
 try{
  if($action==='finalise_expired'){
   if(!$canProcure)throw new RuntimeException('Only procurement/custodian can process expired auctions.');
   $countQuery=$pdo->prepare("SELECT COUNT(*) FROM auctions WHERE status IN ('scheduled','open') AND ends_at IS NOT NULL AND ends_at<=:now");
   $countQuery->execute(['now'=>auction_now()]);
   $eligibleBefore=(int)$countQuery->fetchColumn();
   finalise_expired_auctions(false);
   $countQuery->execute(['now'=>auction_now()]);
   $eligibleAfter=(int)$countQuery->fetchColumn();
   $processed=max(0,$eligibleBefore-$eligibleAfter);
   audit_log((int)$user['id'],'auction.expired_finalisation_triggered','auction','batch',['processed'=>$processed]);
   $success=$processed>0 ? $processed.' expired auction(s) processed. Their provisional results are now in the review queue.' : 'No expired auctions needed finalisation.';
  }else{
  $q=$pdo->prepare("SELECT * FROM auctions WHERE id=:id AND status='closed'");$q->execute(['id'=>$auctionId]);$auction=$q->fetch();
  if(!$auction)throw new RuntimeException('Choose an auction that has closed for bidding.');
  if($action==='disposition'){
   if(!$canProcure)throw new RuntimeException('Only procurement/custodian can disposition unsold items.');
   $resultId=(int)($_POST['result_id']??0);$choice=(string)($_POST['disposition']??'');
   if(!in_array($choice,['pending','held','withdrawn'],true))throw new RuntimeException('Invalid unsold-item action.');
   if(!in_array((string)$auction['results_status'],['pending_review','returned'],true))throw new RuntimeException('Item dispositions can only be changed during procurement review.');
   $q=$pdo->prepare("UPDATE auction_results SET pending_disposition=:choice WHERE id=:rid AND auction_id=:auction AND result_status='unsold' AND pending_disposition<>'relisted'");
   $q->execute(['choice'=>$choice,'rid'=>$resultId,'auction'=>$auctionId]);
   if($q->rowCount()!==1)throw new RuntimeException('That unsold item could not be updated. Reload and try again.');
   audit_log((int)$user['id'],'auction.unsold_disposition','auction_result',(string)$resultId,['auction_id'=>$auctionId,'disposition'=>$choice]);
   $success='Unsold item action saved.';
  }elseif($action==='submit_review'){
   if(!$canProcure)throw new RuntimeException('Only procurement/custodian can submit the close-out review.');
   if(!in_array((string)$auction['results_status'],['pending_review','returned'],true))throw new RuntimeException('This auction is not awaiting procurement review.');
   $comment=trim((string)($_POST['review_comment']??''));
   if(mb_strlen($comment)>2000)throw new RuntimeException('Review notes must be 2,000 characters or fewer.');
   $q=$pdo->prepare("SELECT COUNT(*) FROM auction_results WHERE auction_id=:auction AND result_status='unsold' AND pending_disposition='not_reviewed'");
   $q->execute(['auction'=>$auctionId]);
   if((int)$q->fetchColumn()>0)throw new RuntimeException('Choose Pending Items, Hold, or Withdraw for every unsold lot before submitting.');
   $q=$pdo->prepare("UPDATE auctions SET results_status='awaiting_approval',procurement_reviewed_by=:actor,procurement_reviewed_at=:now,procurement_review_comment=:comment,results_approved_by=NULL,results_approved_at=NULL,results_approval_comment=NULL,updated_at=:now WHERE id=:id AND results_status IN ('pending_review','returned')");
   $q->execute(['actor'=>$user['id'],'now'=>auction_now(),'comment'=>$comment!==''?$comment:null,'id'=>$auctionId]);
   if($q->rowCount()!==1)throw new RuntimeException('The review state changed. Reload the page.');
   audit_log((int)$user['id'],'auction.closeout_submitted','auction',(string)$auctionId,['comment'=>$comment]);
   $success='Close-out review submitted for independent approval.';
  }elseif($action==='approve'){
   if(!$canApprove)throw new RuntimeException('You do not have approval permission.');
   $comment=trim((string)($_POST['approval_comment']??''));
   if(mb_strlen($comment)>2000)throw new RuntimeException('Approval notes must be 2,000 characters or fewer.');
   release_auction_results($auctionId,(int)$user['id'],$comment);
   $success='Results approved and released to staff. Winner notifications and emails can now be processed.';
  }elseif($action==='return'){
   if(!$canApprove)throw new RuntimeException('You do not have approval permission.');
   $comment=trim((string)($_POST['approval_comment']??''));
   if($comment==='')throw new RuntimeException('Add a reason when returning the close-out for correction.');
   if(mb_strlen($comment)>2000)throw new RuntimeException('Approval notes must be 2,000 characters or fewer.');
   if((int)($auction['procurement_reviewed_by']??0)===(int)$user['id'])throw new RuntimeException('A different person must review the close-out.');
   $q=$pdo->prepare("UPDATE auctions SET results_status='returned',results_approved_by=:actor,results_approved_at=:now,results_approval_comment=:comment,updated_at=:now WHERE id=:id AND results_status='awaiting_approval'");
   $q->execute(['actor'=>$user['id'],'now'=>auction_now(),'comment'=>$comment,'id'=>$auctionId]);
   if($q->rowCount()!==1)throw new RuntimeException('This close-out is no longer awaiting approval.');
   audit_log((int)$user['id'],'auction.closeout_returned','auction',(string)$auctionId,['comment'=>$comment]);
   $success='Close-out returned to procurement with your notes.';
  }else{throw new RuntimeException('Unknown action.');}
  }
 }catch(Throwable $e){$error=$e->getMessage();}
}
$auctions=db()->query("SELECT a.id,a.title,a.status,a.ends_at,a.results_status,a.procurement_reviewed_at,a.results_approved_at,(SELECT COUNT(*) FROM auction_results r WHERE r.auction_id=a.id) AS lots,(SELECT COUNT(*) FROM auction_results r WHERE r.auction_id=a.id AND r.result_status='sold') AS sold,(SELECT COUNT(*) FROM auction_results r WHERE r.auction_id=a.id AND r.result_status='unsold') AS unsold FROM auctions a WHERE a.status='closed' ORDER BY a.closed_at DESC,a.id DESC")->fetchAll();
$expiredQuery=db()->prepare("SELECT COUNT(*) FROM auctions WHERE status IN ('scheduled','open') AND ends_at IS NOT NULL AND ends_at<=:now");
$expiredQuery->execute(['now'=>auction_now()]);
$expiredCount=(int)$expiredQuery->fetchColumn();
$selected=null;$results=[];
if($auctionId>0){$q=db()->prepare("SELECT * FROM auctions WHERE id=:id AND status='closed'");$q->execute(['id'=>$auctionId]);$selected=$q->fetch()?:null;
 if($selected){$q=db()->prepare("SELECT r.id AS result_id,r.result_status,r.final_price_cents,r.finalized_at,r.pending_disposition,r.relisted_item_id,i.id AS item_id,i.lot_number,i.title,i.category,i.opening_price_cents,u.display_name AS winner_name FROM auction_results r JOIN auction_items i ON i.id=r.auction_item_id LEFT JOIN users u ON u.id=r.winner_user_id WHERE r.auction_id=:id ORDER BY CAST(i.lot_number AS INTEGER),i.lot_number");$q->execute(['id'=>$auctionId]);$results=$q->fetchAll();}}
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Auction close-out','closeout',$user);
?>
<section class="welcome"><div><p class="eyebrow">PROCUREMENT CONTROL</p><h1>Auction close-out</h1><p class="muted">Bidding closes automatically at the deadline. Procurement reviews provisional outcomes, then a different approver releases results to staff.</p></div><div class="welcome-icon">✓</div></section>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?><?php if($success!==''): ?><div class="notice"><span class="notice-dot"></span><div><strong>Update saved</strong><p><?= e($success) ?></p></div></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">CLOSED AUCTIONS</p><h2>Review queue</h2></div><span class="status-pill"><?= count($auctions) ?> auction(s)</span></div>
<?php if($canProcure): ?><div class="inline-form" style="justify-content:space-between;margin:0 0 1rem"><p class="form-help"><?= $expiredCount>0 ? $expiredCount.' expired auction(s) ready to process.' : 'No expired auctions are waiting to be processed.' ?></p><?php if($expiredCount>0): ?><form method="post" onsubmit="return confirm('Process all expired auctions now? This records provisional results and closes them for bidding.');"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="finalise_expired"><button class="button button-primary" type="submit">Process expired auctions →</button></form><?php endif; ?></div><?php endif; ?>
<?php if(!$auctions): ?><div class="empty-state"><strong>No closed auctions yet</strong><p>When an auction passes its end time, a procurement user or custodian can click “Process expired auctions” above to prepare its provisional results.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><th>Auction</th><th>Lots</th><th>Sold</th><th>Unsold</th><th>Close-out status</th><th></th></tr></thead><tbody><?php foreach($auctions as $a): ?><tr><td><strong><?= e($a['title']) ?></strong><small class="table-sub">Closed <?= e($a['ends_at']??'') ?></small></td><td><?= (int)$a['lots'] ?></td><td><?= (int)$a['sold'] ?></td><td><?= (int)$a['unsold'] ?></td><td><span class="status-pill"><?= e(str_replace('_',' ',ucfirst((string)$a['results_status']))) ?></span></td><td><a class="text-link" href="<?= e(app_url('admin/closeout.php?auction='.(int)$a['id'])) ?>">Review →</a></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?></section>
<?php if($selected): ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">PROVISIONAL RESULTS</p><h2><?= e($selected['title']) ?></h2><p class="muted">Status: <?= e(str_replace('_',' ',(string)$selected['results_status'])) ?> · Finalised lots are not visible to employees until release.</p></div><span class="status-pill"><?= count($results) ?> lot(s)</span></div>
<?php if(in_array((string)$selected['results_status'],['awaiting_approval','released'],true)): ?><div class="notice"><span class="notice-dot"></span><div><strong><?= $selected['results_status']==='released'?'Results released':'Awaiting independent approval' ?></strong><p>Procurement review: <?= e($selected['procurement_reviewed_at']??'—') ?>. <?= $selected['results_status']==='released'?'Released: '.e($selected['results_approved_at']??'—'):'' ?></p><?php if(!empty($selected['procurement_review_comment'])): ?><p>Procurement notes: <?= nl2br(e($selected['procurement_review_comment'])) ?></p><?php endif; ?><?php if(!empty($selected['results_approval_comment'])): ?><p>Approver notes: <?= nl2br(e($selected['results_approval_comment'])) ?></p><?php endif; ?></div></div><?php endif; ?>
<div class="table-wrap"><table class="data-table"><thead><tr><th>Lot / item</th><th>Outcome</th><th>Winner / final price</th><th>Unsold-item action</th></tr></thead><tbody><?php foreach($results as $r): ?><tr><td><strong>Lot <?= e($r['lot_number']) ?> · <?= e($r['title']) ?></strong><small class="table-sub"><?= e($r['category']??'Uncategorised') ?></small></td><td><?= e(ucfirst((string)$r['result_status'])) ?></td><td><?php if($r['result_status']==='sold'): ?><?= e($r['winner_name']??'Winner recorded') ?><small class="table-sub">N$ <?= number_format((int)$r['final_price_cents']/100,2,'.',',') ?></small><?php else: ?><span class="muted">No bids</span><?php endif; ?></td><td><?php if($r['result_status']!=='unsold'): ?>—<?php elseif($r['pending_disposition']==='relisted'): ?><span class="status-pill">Relisted as new draft</span><?php elseif($canProcure&&in_array((string)$selected['results_status'],['pending_review','returned'],true)): ?><form method="post" class="inline-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="disposition"><input type="hidden" name="auction_id" value="<?= (int)$selected['id'] ?>"><input type="hidden" name="result_id" value="<?= (int)$r['result_id'] ?>"><select name="disposition" aria-label="Unsold item action"><option value="not_reviewed" disabled <?= $r['pending_disposition']==='not_reviewed'?'selected':'' ?>>Choose action</option><option value="pending" <?= $r['pending_disposition']==='pending'?'selected':'' ?>>Pending Items</option><option value="held" <?= $r['pending_disposition']==='held'?'selected':'' ?>>Hold</option><option value="withdrawn" <?= $r['pending_disposition']==='withdrawn'?'selected':'' ?>>Withdraw</option></select><button class="button button-quiet" type="submit">Save</button></form><?php else: ?><span class="status-pill"><?= e(str_replace('_',' ',ucfirst((string)$r['pending_disposition']))) ?></span><?php endif; ?></td></tr><?php endforeach; ?></tbody></table></div>
<?php if($canProcure&&in_array((string)$selected['results_status'],['pending_review','returned'],true)): ?><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="action" value="submit_review"><input type="hidden" name="auction_id" value="<?= (int)$selected['id'] ?>"><label for="review_comment">Procurement review notes</label><textarea id="review_comment" name="review_comment" rows="3" maxlength="2000" placeholder="Summary of the review, exceptions, and follow-up actions"></textarea><p class="form-help">Every unsold lot must be marked Pending Items, Hold or Withdraw before submission. An approver other than the reviewer must make the final decision.</p><button class="button button-primary" type="submit">Submit for approval →</button></form><?php endif; ?>
<?php if($canApprove&&$selected['results_status']==='awaiting_approval'): ?><form method="post" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="auction_id" value="<?= (int)$selected['id'] ?>"><label for="approval_comment">Approval notes</label><textarea id="approval_comment" name="approval_comment" rows="3" maxlength="2000" placeholder="Optional approval note; required if returning for correction"></textarea><div class="inline-form"><button class="button button-primary" name="action" value="approve" type="submit">Approve and release results</button><button class="button button-quiet" name="action" value="return" type="submit">Return for correction</button></div><p class="form-help">Approval releases the results to staff and allows winner notifications/email delivery. The procurement reviewer cannot approve their own close-out.</p></form><?php endif; ?>
</section><?php endif; ?>
<p><a class="text-link" href="<?= e(app_url('admin/pending-items.php')) ?>">Open Pending Items →</a></p>
<?php render_page_end(); ?>
