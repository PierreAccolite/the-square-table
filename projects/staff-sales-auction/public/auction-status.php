<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require dirname(__DIR__).'/app/auction_engine.php';
$user = require_login();
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, max-age=0');
header('X-Content-Type-Options: nosniff');
$itemId = filter_input(INPUT_GET, 'id', FILTER_VALIDATE_INT);
if (!$itemId) {
    http_response_code(400);
    echo json_encode(['error'=>'Invalid item id']);
    exit;
}
try {
    $pdo = db();
    auction_close_expired($pdo);
    $q = $pdo->prepare("SELECT i.id,i.status AS item_status,i.opening_price_cents,i.minimum_increment_cents,a.status AS auction_status,a.starts_at,a.ends_at,(SELECT MAX(b.amount_cents) FROM bids b WHERE b.auction_item_id=i.id) AS high_bid,(SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.id=:id");
    $q->execute(['id'=>(int)$itemId]);
    $row = $q->fetch();
    if (!$row) {
        http_response_code(404);
        echo json_encode(['error'=>'Item not found']);
        exit;
    }
    $current = $row['high_bid'] === null ? (int)$row['opening_price_cents'] : (int)$row['high_bid'];
    $minimum = $row['high_bid'] === null ? (int)$row['opening_price_cents'] : (int)$row['high_bid'] + (int)$row['minimum_increment_cents'];
    $now = auction_engine_now();
    $canBid = $row['item_status']==='published'
        && in_array($row['auction_status'],['scheduled','open'],true)
        && !empty($row['starts_at']) && $row['starts_at'] <= $now
        && !empty($row['ends_at']) && $row['ends_at'] > $now
        && has_permission($user,'bidder');
    echo json_encode([
        'item_id'=>(int)$row['id'],
        'auction_status'=>$row['auction_status'],
        'item_status'=>$row['item_status'],
        'current_price_cents'=>$current,
        'minimum_bid_cents'=>$minimum,
        'bid_count'=>(int)$row['bid_count'],
        'ends_at'=>$row['ends_at'],
        'can_bid'=>$canBid
    ], JSON_THROW_ON_ERROR);
} catch (Throwable $e) {
    error_log('Staff Sales status refresh failed: '.$e->getMessage());
    http_response_code(503);
    echo json_encode(['error'=>'Status temporarily unavailable']);
}
