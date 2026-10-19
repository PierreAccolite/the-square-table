<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
$q=db()->prepare("SELECT a.id AS auction_id,a.title AS auction_title,a.closed_at,i.id AS item_id,i.lot_number,i.title AS item_title,r.result_status,r.winner_user_id,r.final_price_cents,r.finalised_at FROM auction_item_results r JOIN auctions a ON a.id=r.auction_id JOIN auction_items i ON i.id=r.auction_item_id WHERE a.status='closed' ORDER BY a.closed_at DESC,a.id DESC,CAST(i.lot_number AS INTEGER),i.lot_number");
$q->execute();
$rows=$q->fetchAll();
require dirname(__DIR__).'/app/layout.php';
render_page_start('Auction results','results',$user);
?>
<section class="welcome"><div><p class="eyebrow">STAFF SALES · RESULTS</p><h1>Auction results</h1><p class="muted">Final outcomes for closed staff sales. Other employees' identities are kept private.</p></div><div class="welcome-icon">✓</div></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">FINAL RESULTS</p><h2>Closed auctions</h2></div><span class="status-pill"><?= count($rows) ?> item result(s)</span></div>
<?php if(!$rows): ?><div class="empty-state"><strong>No final results yet</strong><p>Results appear here after an auction has closed and its lots have been finalised.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><th>Auction</th><th>Lot</th><th>Item</th><th>Outcome</th><th>Final price</th><th>Your result</th></tr></thead><tbody><?php foreach($rows as $row): ?><tr><td><?= e($row['auction_title']) ?><small class="table-sub"><?= e($row['closed_at']??'') ?></small></td><td><?= e($row['lot_number']) ?></td><td><?= e($row['item_title']) ?></td><td><span class="status-pill"><?= e(ucfirst($row['result_status'])) ?></span></td><td><?= $row['final_price_cents']===null?'—':'N$ '.number_format((int)$row['final_price_cents']/100,2,'.',',') ?></td><td><?php if($row['result_status']==='sold'&&(int)$row['winner_user_id']===(int)$user['id']): ?><strong>You won this lot</strong><?php elseif($row['result_status']==='unsold'): ?>No accepted bids<?php else: ?>Sold — winner details private<?php endif; ?></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?>
<p class="form-help">This page shows finalised outcomes only. Detailed winner identities and bid-level exports are restricted to authorised administrators and auditors.</p></section>
<?php render_page_end(); ?>
