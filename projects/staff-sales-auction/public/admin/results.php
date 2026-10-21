<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
require dirname(__DIR__,2).'/app/auction_engine.php';
$user=require_role('admin');
$rows=db()->query(
 "SELECT r.id,r.result_status,r.final_price_cents,r.finalized_at,r.email_status,r.email_sent_at,r.email_last_error,
         a.title AS auction_title,a.results_status,i.lot_number,i.title AS item_title,
         u.display_name AS winner_name,u.username AS winner_username,u.email AS winner_email
  FROM auction_results r JOIN auctions a ON a.id=r.auction_id
  JOIN auction_items i ON i.id=r.auction_item_id
  LEFT JOIN users u ON u.id=r.winner_user_id
  ORDER BY r.finalized_at DESC,a.title,i.lot_number"
)->fetchAll();
$q=db()->prepare("SELECT COUNT(*) FROM auctions WHERE status IN ('scheduled','open') AND ends_at IS NOT NULL AND ends_at<=:now");$q->execute(['now'=>auction_now()]);$pending=(int)$q->fetchColumn();
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Auction results','results',$user);
?>
<section class="welcome"><div><p class="eyebrow">AUCTION CLOSE & AWARDS</p><h1>Auction results</h1><p class="muted">Finalised item outcomes, winner notification status and closing checks.</p></div><div class="welcome-icon">🏆</div></section>
<?php if($pending>0): ?><div class="notice"><span class="notice-dot"></span><div><strong><?= $pending ?> auction(s) are past their closing time</strong><p>Run the scheduled CLI finalisation job to close them and generate results. This page does not close auctions itself.</p></div></div><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">FINAL RESULTS</p><h2>Item outcomes</h2></div><span class="status-pill"><?= count($rows) ?> result(s)</span></div>
<?php if(!$rows): ?><div class="empty-state"><strong>No auctions have been finalised yet</strong><p>Once an auction end time has passed, the scheduled finalisation job records each published item as sold or unsold.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><th>Auction / item</th><th>Close-out</th><th>Outcome</th><th>Winner</th><th>Final price</th><th>Finalised</th><th>Winner email</th></tr></thead><tbody><?php foreach($rows as $r): ?><tr><td><strong><?= e($r['auction_title']) ?></strong><small class="table-sub">Lot <?= e($r['lot_number']) ?> · <?= e($r['item_title']) ?></small></td><td><span class="status-pill"><?= e(str_replace('_',' ',ucfirst((string)$r['results_status']))) ?></span></td><td><?= e(ucfirst($r['result_status'])) ?></td><td><?= e($r['winner_name']??'—') ?><?php if(!empty($r['winner_username'])): ?><small class="table-sub"><?= e($r['winner_username']) ?></small><?php endif; ?></td><td>N$ <?= number_format((int)$r['final_price_cents']/100,2,'.',',') ?></td><td><?= e($r['finalized_at']) ?></td><td><span class="status-pill"><?= e(str_replace('_',' ',ucfirst($r['email_status']))) ?></span><?php if($r['email_last_error']): ?><small class="table-sub"><?= e($r['email_last_error']) ?></small><?php endif; ?></td></tr><?php endforeach; ?></tbody></table></div><?php endif; ?>
<p class="form-help">Winner names and email status are restricted to administrators. Staff see only their own results. Email delivery depends on the server's configured mail transport.</p></section>
<?php render_page_end(); ?>
