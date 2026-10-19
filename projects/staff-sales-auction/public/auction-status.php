<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require dirname(__DIR__).'/app/auction_engine.php';
$user = require_login();
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, max-age=0');
header('X-Content-Type-Options: nosniff');
$raw = isset($_GET['ids']) ? (string)$_GET['ids'] : (string)($_GET['id'] ?? '');
$ids = array_values(array_unique(array_filter(array_map('intval', explode(',', $raw)), static fn($v)=>$v>0)));
if (!$ids || count($ids)>100) {
    http_response_code(400);
    echo json_encode(['error'=>'Provide between 1 and 100 valid item ids']);
    exit;
}
try {
    $pdo = db();
    auction_close_expired($pdo);
    $marks = implode(',', array_fill(0,count($ids),'?'));
    $q = $pdo->prepare("SELECT i.id,i.status AS item_status,i.opening_price_cents,i.minimum_increment_cents,a.status AS auction_status,a.starts_at,a.ends_at,(SELECT MAX(b.amount_cents) FROM bids b WHERE b.auction_item_id=i.id) AS high_bid,(SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.id IN ($marks)");
    $q->execute($ids);
    $result = [];
    $now = auction_engine_now();
    foreach ($q->fetchAll() as $row) {
        $current = $row['high_bid'] === null ? (int)$row['opening_price_cents'] : (int)$row['high_bid'];
        $minimum = $row['high_bid'] === null ? (int)$row['opening_price_cents'] : (int)$row['high_bid'] + (int)$row['minimum_increment_cents'];
        $canBid = $row['item_status']==='published'
            && in_array($row['auction_status'],['scheduled','open'],true)
            && !empty($row['starts_at']) && $row['starts_at'] <= $now
            && !empty($row['ends_at']) && $row['ends_at'] > $now
            && has_permission($user,'bidder');
        $result[(string)(int)$row['id']] = [
            'item_id'=>(int)$row['id'],
            'auction_status'=>$row['auction_status'],
            'item_status'=>$row['item_status'],
            'current_price_cents'=>$current,
            'minimum_bid_cents'=>$minimum,
            'bid_count'=>(int)$row['bid_count'],
            'ends_at'=>$row['ends_at'],
            'can_bid'=>$canBid
        ];
    }
    echo json_encode(['items'=>(object)$result], JSON_THROW_ON_ERROR);
} catch (Throwable $e) {
    error_log('Staff Sales status refresh failed: '.$e->getMessage());
    http_response_code(503);
    echo json_encode(['error'=>'Status temporarily unavailable']);
}
