<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
$q=db()->prepare(
 "SELECT i.id AS item_id,i.lot_number,i.title AS item_title,a.title AS auction_title,a.status AS auction_status,a.results_status,
         r.result_status,r.final_price_cents,r.finalized_at,
         MAX(own.amount_cents) AS my_high_bid,
         CASE WHEN r.winner_user_id=:uid_winner THEN 1 ELSE 0 END AS is_winner
  FROM bids own
  JOIN auction_items i ON i.id=own.auction_item_id
  JOIN auctions a ON a.id=i.auction_id
  LEFT JOIN auction_results r ON r.auction_item_id=i.id AND a.results_status='released'
  WHERE own.bidder_user_id=:uid_filter
  GROUP BY i.id,r.id
  ORDER BY COALESCE(r.finalized_at,'9999-12-31 23:59:59') DESC,a.title,i.lot_number"
);
$q->execute(['uid_filter'=>(int)$user['id'],'uid_winner'=>(int)$user['id']]);$results=$q->fetchAll();
require dirname(__DIR__).'/app/layout.php';render_page_start('My results','my-results',$user);
?>
<section class="welcome"><div><p class="eyebrow">YOUR AUCTION OUTCOMES</p><h1>My results</h1><p class="muted">Only your own bidding activity and results are shown here.</p></div><div class="welcome-icon">🏆</div></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">RESULTS</p><h2>Your auction items</h2></div><span class="status-pill"><?= count($results) ?> item(s)</span></div>
<?php if(!$results): ?><div class="empty-state"><strong>No results to show yet</strong><p>Items you have bid on will appear here. Results become final after the auction closing process completes.</p><a class="button button-primary" href="<?= e(app_url('auctions.php')) ?>">Browse catalogue →</a></div><?php else: ?>
<div class="table-wrap"><table class="data-table"><thead><tr><th>Item</th><th>Auction</th><th>Your highest bid</th><th>Outcome</th><th>Final price</th><th>Finalised</th></tr></thead><tbody>
<?php foreach($results as $r): ?><tr><td><strong><?= e($r['item_title']) ?></strong><small class="table-sub">Lot <?= e($r['lot_number']) ?></small></td><td><?= e($r['auction_title']) ?></td><td>N$ <?= number_format((int)$r['my_high_bid']/100,2,'.',',') ?></td><td><?php if($r['result_status']===null): ?><span class="status-pill"><?= $r['auction_status']==='closed'?'Awaiting approved release':'Awaiting finalisation' ?></span><?php elseif((int)$r['is_winner']===1): ?><span class="status-pill">Won</span><?php else: ?><span class="status-pill">Not won</span><?php endif; ?></td><td><?= $r['result_status']===null?'—':'N$ '.number_format((int)$r['final_price_cents']/100,2,'.',',') ?></td><td><?= e($r['finalized_at']??'Pending') ?></td></tr><?php endforeach; ?>
</tbody></table></div><?php endif; ?></section>
<?php render_page_end(); ?>
