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
];
$report=(string)($_GET['report']??'auction_summary');
$allowed=['auction_summary','item_catalogue','bid_activity','user_accounts'];
if(!in_array($report,$allowed,true))$report='auction_summary';
if(isset($_GET['export'])&&$_GET['export']==='csv'){
 $queries=[
 'auction_summary'=>['Auction summary','SELECT a.title AS auction, a.status, a.starts_at, a.ends_at, COUNT(DISTINCT i.id) AS item_count, COUNT(b.id) AS bid_count FROM auctions a LEFT JOIN auction_items i ON i.auction_id=a.id LEFT JOIN bids b ON b.auction_item_id=i.id GROUP BY a.id ORDER BY a.created_at DESC',],
 'item_catalogue'=>['Item catalogue',"SELECT a.title AS auction,i.lot_number,i.title AS item,i.status,i.opening_price_cents/100.0 AS opening_price_NAD,i.minimum_increment_cents/100.0 AS increment_NAD,COUNT(b.id) AS bid_count FROM auction_items i JOIN auctions a ON a.id=i.auction_id LEFT JOIN bids b ON b.auction_item_id=i.id GROUP BY i.id ORDER BY a.title,i.lot_number"],
 'bid_activity'=>['Bid activity',"SELECT a.title AS auction,i.lot_number,i.title AS item,u.username AS bidder_username,u.display_name AS bidder_name,b.amount_cents/100.0 AS bid_NAD,b.placed_at FROM bids b JOIN auction_items i ON i.id=b.auction_item_id JOIN auctions a ON a.id=i.auction_id JOIN users u ON u.id=b.bidder_user_id ORDER BY b.placed_at DESC,b.id DESC"],
 'user_accounts'=>['User accounts','SELECT username,display_name,employee_number,role,status,created_at,last_login_at FROM users ORDER BY display_name COLLATE NOCASE'],
 ];
 [$label,$sql]=$queries[$report];$rows=$pdo->query($sql);header('Content-Type: text/csv; charset=utf-8');header('Content-Disposition: attachment; filename="staff-sales-'.str_replace('_','-',$report).'-'.date('Y-m-d').'.csv"');header('X-Content-Type-Options: nosniff');$out=fopen('php://output','wb');fputcsv($out,[$label]);$first=$rows->fetch(PDO::FETCH_ASSOC);if($first){fputcsv($out,array_keys($first));$write=static function($row)use($out){fputcsv($out,array_map(static function($v){$s=(string)($v??'');return preg_match('/^[=+@\-]/',$s)?"'".$s:$s;},array_values($row)));};$write($first);while($row=$rows->fetch(PDO::FETCH_ASSOC))$write($row);}fclose($out);exit;
}
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Reports','reports',$user);
$labels=['auction_summary'=>'Auction summary','item_catalogue'=>'Item catalogue','bid_activity'=>'Bid activity','user_accounts'=>'User accounts'];
?>
<section class="welcome"><div><p class="eyebrow">OVERVIEW & EXPORTS</p><h1>Reports</h1><p class="muted">Read-only operational reports. Bid activity is empty until live bidding is implemented.</p></div><div class="welcome-icon">▥</div></section>
<section class="stat-grid"><article class="stat-card"><span class="stat-label">User accounts</span><strong><?= $summary['users'] ?></strong></article><article class="stat-card"><span class="stat-label">Active accounts</span><strong><?= $summary['active_users'] ?></strong></article><article class="stat-card"><span class="stat-label">Auctions</span><strong><?= $summary['auctions'] ?></strong></article><article class="stat-card"><span class="stat-label">Items</span><strong><?= $summary['items'] ?></strong></article><article class="stat-card"><span class="stat-label">Bid records</span><strong><?= $summary['bids'] ?></strong></article></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">CSV REPORTS</p><h2>Choose a report</h2></div></div><div class="action-grid"><?php foreach($labels as $key=>$label): ?><a class="action-card" href="<?= e(app_url('admin/reports.php?report='.$key)) ?>"><span class="action-icon">▤</span><strong><?= e($label) ?></strong><span><?php if($key==='auction_summary'): ?>Auction dates, statuses, catalogue totals and bid counts.<?php elseif($key==='item_catalogue'): ?>Lot status, opening price, increment and bids per item.<?php elseif($key==='bid_activity'): ?>Detailed bid history, including bidder details; administrator/auditor access only.<?php else: ?>Staff account list and last sign-in.<?php endif; ?></span><b>Preview report →</b></a><?php endforeach; ?></div></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">EXPORT</p><h2><?= e($labels[$report]) ?></h2></div><a class="button button-primary" href="<?= e(app_url('admin/reports.php?report='.$report.'&export=csv')) ?>">Export CSV ↓</a></div><p class="form-help">Exports are generated from the current local database. Keep files containing staff names or bid history in approved storage.</p></section>
<?php render_page_end(); ?>