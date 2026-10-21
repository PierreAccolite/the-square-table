<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
$rows=db()->query("SELECT a.id AS auction_id,a.title AS auction_title,a.closed_at,a.results_approved_at,i.lot_number,i.title AS item_title,i.category,r.result_status,r.final_price_cents,r.finalized_at FROM auction_results r JOIN auctions a ON a.id=r.auction_id JOIN auction_items i ON i.id=r.auction_item_id WHERE a.status='closed' AND a.results_status='released' ORDER BY COALESCE(a.results_approved_at,a.closed_at) DESC,a.title,CAST(i.lot_number AS INTEGER),i.lot_number")->fetchAll();
$grouped=[];
foreach($rows as $row){$id=(int)$row['auction_id'];if(!isset($grouped[$id]))$grouped[$id]=['title'=>$row['auction_title'],'released_at'=>$row['results_approved_at']??$row['closed_at'],'items'=>[]];$grouped[$id]['items'][]=$row;}
require dirname(__DIR__).'/app/layout.php';render_page_start('Released auction results','released-results',$user);
?>
<section class="welcome"><div><p class="eyebrow">APPROVED OUTCOMES</p><h1>Auction results</h1><p class="muted">Final results are published here only after procurement review and independent approval. Winner identities are not displayed.</p></div><div class="welcome-icon">🏆</div></section>
<?php if(!$grouped): ?><section class="content-panel empty-state"><strong>No results have been released yet</strong><p>Approved auction results will appear here after procurement has completed the close-out review and an authorised approver releases them.</p></section><?php else: ?>
<?php foreach($grouped as $auction): ?><section class="content-panel"><div class="section-heading"><div><p class="eyebrow">RELEASED RESULTS</p><h2><?= e($auction['title']) ?></h2><p class="muted">Released <?= e($auction['released_at']??'') ?></p></div><span class="status-pill"><?= count($auction['items']) ?> lot(s)</span></div><div class="table-wrap"><table class="data-table"><thead><tr><th>Lot / item</th><th>Category</th><th>Outcome</th><th>Final price</th></tr></thead><tbody><?php foreach($auction['items'] as $item): ?><tr><td><strong>Lot <?= e($item['lot_number']) ?> · <?= e($item['item_title']) ?></strong></td><td><?= e($item['category']??'Uncategorised') ?></td><td><span class="status-pill"><?= e($item['result_status']==='sold'?'Sold':'Unsold') ?></span></td><td><?= $item['result_status']==='sold'?'N$ '.number_format((int)$item['final_price_cents']/100,2,'.',','):'—' ?></td></tr><?php endforeach; ?></tbody></table></div></section><?php endforeach; ?>
<?php endif; ?>
<?php render_page_end(); ?>
