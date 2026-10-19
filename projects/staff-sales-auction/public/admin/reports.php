<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin','auditor');
$pdo=db();
$summary=[
 'users'=>(int)$pdo->query('SELECT COUNT(*) FROM users')->fetchColumn(),
 'active_users'=>(int)$pdo->query("SELECT COUNT(*) FROM users WHERE status='active'")->fetchColumn(),
 'auctions'=>(int)$pdo->query('SELECT COUNT(*) FROM auctions')->fetchColumn(),
 'items'=>(int)$pdo->query('SELECT COUNT(*) FROM auction_items')->fetchColumn(),
 'bids'=>(int)$pdo->query('SELECT COUNT(*) FROM bids')->fetchColumn(),
 'results'=>(int)$pdo->query('SELECT COUNT(*) FROM auction_results')->fetchColumn(),
];
$report=(string)($_GET['report']??'auction_summary');
$queries=[
 'auction_summary'=>['Auction summary',"SELECT a.title AS auction,a.status,a.starts_at,a.ends_at,(SELECT COUNT(*) FROM auction_items i WHERE i.auction_id=a.id) AS item_count,(SELECT COUNT(*) FROM bids b JOIN auction_items i ON i.id=b.auction_item_id WHERE i.auction_id=a.id) AS bid_count,(SELECT COUNT(*) FROM auction_results r WHERE r.auction_id=a.id) AS finalised_items FROM auctions a ORDER BY a.created_at DESC"],
 'item_catalogue'=>['Item catalogue',"SELECT a.title AS auction,i.lot_number,i.title AS item,i.status,i.opening_price_cents/100.0 AS opening_price_NAD,i.minimum_increment_cents/100.0 AS increment_NAD,(SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count FROM auction_items i JOIN auctions a ON a.id=i.auction_id ORDER BY a.title,i.lot_number"],
 'bid_activity'=>['Bid activity',"SELECT a.title AS auction,i.lot_number,i.title AS item,u.username AS bidder_username,u.display_name AS bidder_name,b.amount_cents/100.0 AS bid_NAD,b.placed_at FROM bids b JOIN auction_items i ON i.id=b.auction_item_id JOIN auctions a ON a.id=i.auction_id JOIN users u ON u.id=b.bidder_user_id ORDER BY b.placed_at DESC,b.id DESC"],
 'user_accounts'=>['User accounts',"SELECT username,display_name,employee_number,role,status,created_at,last_login_at FROM users ORDER BY display_name COLLATE NOCASE"],
 'auction_results'=>['Auction results',"SELECT a.title AS auction,i.lot_number,i.title AS item,r.result_status,u.username AS winner_username,u.display_name AS winner_name,r.final_price_cents/100.0 AS final_price_NAD,r.finalized_at,r.email_status,r.email_sent_at FROM auction_results r JOIN auction_items i ON i.id=r.auction_item_id JOIN auctions a ON a.id=r.auction_id LEFT JOIN users u ON u.id=r.winner_user_id ORDER BY r.finalized_at DESC,a.title,i.lot_number"],
];
if(!isset($queries[$report]))$report='auction_summary';
[$label,$sql]=$queries[$report];
if(isset($_GET['export'])&&$_GET['export']==='csv'){
 $rows=$pdo->query($sql);
 header('Content-Type: text/csv; charset=utf-8');
 header('Content-Disposition: attachment; filename="staff-sales-'.str_replace('_','-',$report).'-'.date('Y-m-d').'.csv"');
 header('X-Content-Type-Options: nosniff');
 $out=fopen('php://output','wb');fputcsv($out,[$label]);
 $first=$rows->fetch(PDO::FETCH_ASSOC);
 if($first){
  fputcsv($out,array_keys($first));
  $write=static function(array $row)use($out):void{
   fputcsv($out,array_map(static function($v):string{$s=(string)($v??'');return preg_match('/^[=+@\-]/',$s)?"'".$s:$s;},array_values($row)));
  };
  $write($first);while($row=$rows->fetch(PDO::FETCH_ASSOC))$write($row);
 }
 fclose($out);exit;
}
$previewRows=$pdo->query($sql.' LIMIT 100')->fetchAll(PDO::FETCH_ASSOC);
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Reports','reports',$user);
$labels=array_map(static fn($v)=>$v[0],$queries);
?>
<section class="welcome"><div><p class="eyebrow">OVERVIEW & EXPORTS</p><h1>Reports</h1><p class="muted">Read-only operational reports covering bids, finalised results and account activity.</p></div><div class="welcome-icon">▥</div></section>
<section class="stat-grid"><article class="stat-card"><span class="stat-label">User accounts</span><strong><?= $summary['users'] ?></strong></article><article class="stat-card"><span class="stat-label">Active accounts</span><strong><?= $summary['active_users'] ?></strong></article><article class="stat-card"><span class="stat-label">Auctions</span><strong><?= $summary['auctions'] ?></strong></article><article class="stat-card"><span class="stat-label">Items</span><strong><?= $summary['items'] ?></strong></article><article class="stat-card"><span class="stat-label">Bid records</span><strong><?= $summary['bids'] ?></strong></article><article class="stat-card"><span class="stat-label">Finalised items</span><strong><?= $summary['results'] ?></strong></article></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">CSV REPORTS</p><h2>Choose a report</h2></div></div><div class="action-grid"><?php foreach($labels as $key=>$label): ?><a class="action-card" href="<?= e(app_url('admin/reports.php?report='.$key)) ?>"><span class="action-icon">▤</span><strong><?= e($label) ?></strong><span><?php if($key==='auction_summary'): ?>Auction dates, status, catalogue totals, bids and finalised item counts.<?php elseif($key==='item_catalogue'): ?>Lot status, opening price, increments and bid counts.<?php elseif($key==='bid_activity'): ?>Detailed bid history including bidder identities.<?php elseif($key==='auction_results'): ?>Final outcome, winner, final price and notification status.<?php else: ?>Staff account list and last sign-in.<?php endif; ?></span><b>Preview report →</b></a><?php endforeach; ?></div></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">REPORT PREVIEW</p><h2><?= e($label) ?></h2><p class="muted">Showing up to 100 rows.</p></div><a class="button button-primary" href="<?= e(app_url('admin/reports.php?report='.$report.'&export=csv')) ?>">Export CSV ↓</a></div><?php if(!$previewRows): ?><div class="empty-state"><strong>No records to show</strong><p>This report will populate as auctions, bids and final results are recorded.</p></div><?php else: ?><div class="table-wrap"><table class="data-table"><thead><tr><?php foreach(array_keys($previewRows[0]) as $heading): ?><th><?= e(str_replace('_',' ',ucfirst((string)$heading))) ?></th><?php endforeach; ?></tr></thead><tbody><?php foreach($previewRows as $row): ?><tr><?php foreach($row as $value): ?><td><?= e($value??'—') ?></td><?php endforeach; ?></tr><?php endforeach; ?></tbody></table></div><?php endif; ?><p class="form-help">Exports are generated from the current local database. Keep files containing staff names or bid history in approved storage.</p></section>
<?php render_page_end(); ?>
