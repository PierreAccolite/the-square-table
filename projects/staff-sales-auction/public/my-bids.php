<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
$q=db()->prepare(
 "SELECT b.id,b.amount_cents,b.placed_at,
         i.id AS item_id,i.lot_number,i.title AS item_title,
         a.title AS auction_title,a.status AS auction_status,a.ends_at,
         (SELECT MAX(b2.amount_cents) FROM bids b2 WHERE b2.auction_item_id=i.id) AS current_high,
         (SELECT MAX(b3.amount_cents) FROM bids b3 WHERE b3.auction_item_id=i.id AND b3.bidder_user_id=:uid) AS my_high,
         (SELECT COUNT(*) FROM bids b4 WHERE b4.auction_item_id=i.id AND b4.bidder_user_id=:uid) AS my_bid_count
  FROM bids b
  JOIN auction_items i ON i.id=b.auction_item_id
  JOIN auctions a ON a.id=i.auction_id
  WHERE b.bidder_user_id=:uid
  ORDER BY b.placed_at DESC,b.id DESC"
);
$q->execute(['uid'=>(int)$user['id']]);
$allBids=$q->fetchAll();
$items=[];
foreach($allBids as $bid){
 $id=(int)$bid['item_id'];
 if(!isset($items[$id])){
  $items[$id]=[
   'item_id'=>$id,'lot_number'=>$bid['lot_number'],'item_title'=>$bid['item_title'],
   'auction_title'=>$bid['auction_title'],'auction_status'=>$bid['auction_status'],
   'ends_at'=>$bid['ends_at'],'current_high'=>$bid['current_high']===null?null:(int)$bid['current_high'],
   'my_high'=>(int)$bid['my_high'],'my_bid_count'=>(int)$bid['my_bid_count'],
   'latest_amount'=>(int)$bid['amount_cents'],'latest_time'=>$bid['placed_at'],'history'=>[]
  ];
 }
 $items[$id]['history'][]=$bid;
}
require dirname(__DIR__).'/app/layout.php';
render_page_start('My bids','my-bids',$user);
?>
<section class="welcome"><div><p class="eyebrow">YOUR ACTIVITY</p><h1>My bids</h1><p class="muted">One row per lot, with your latest bid and highest personal bid. Expand a lot to review your earlier bids.</p></div><div class="welcome-icon">↗</div></section>
<div class="notice"><span class="notice-dot"></span><div><strong>Your bid history is private to your account</strong><p>Other bidders' identities are never shown here. Your highest bid is not necessarily the current winning bid.</p><p><a class="text-link" href="<?= e(app_url('my-results.php')) ?>">View my results →</a></p></div></div>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">YOUR LOTS</p><h2>Items you've bid on</h2></div><span class="status-pill"><?= count($items) ?> unique lot(s)</span></div>
<?php if(!$items): ?><div class="empty-state"><strong>You have not placed any bids</strong><p>Accepted bids will appear here after you bid on a catalogue item.</p><a class="button button-primary" href="<?= e(app_url('auctions.php')) ?>">Browse catalogue →</a></div><?php else: ?>
<div class="table-wrap"><table class="data-table"><thead><tr><th>Item / lot</th><th>Auction</th><th>Your latest bid</th><th>Your highest bid</th><th>Current high</th><th>Your bids</th><th>Status / history</th></tr></thead><tbody>
<?php foreach($items as $item): ?><tr><td><strong><?= e($item['item_title']) ?></strong><small class="table-sub">Lot <?= e($item['lot_number']) ?></small></td><td><?= e($item['auction_title']) ?></td><td>N$ <?= number_format($item['latest_amount']/100,2,'.',',') ?><small class="table-sub"><?= e($item['latest_time']) ?></small></td><td>N$ <?= number_format($item['my_high']/100,2,'.',',') ?></td><td><?= $item['current_high']===null?'—':'N$ '.number_format($item['current_high']/100,2,'.',',') ?></td><td><?= $item['my_bid_count'] ?></td><td><span class="status-pill"><?= e(ucfirst($item['auction_status'])) ?></span><details class="bid-history"><summary>My bid history</summary><div class="table-wrap"><table class="data-table"><thead><tr><th>Time placed</th><th>Amount</th></tr></thead><tbody><?php foreach($item['history'] as $history): ?><tr><td><?= e($history['placed_at']) ?></td><td>N$ <?= number_format((int)$history['amount_cents']/100,2,'.',',') ?></td></tr><?php endforeach; ?></tbody></table></div></details></td></tr><?php endforeach; ?>
</tbody></table></div><?php endif; ?></section>
<?php render_page_end(); ?>
