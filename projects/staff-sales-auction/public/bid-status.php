<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require_login();
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('X-Content-Type-Options: nosniff');
$rows=db()->query(
 "SELECT i.id,i.opening_price_cents,i.minimum_increment_cents,
         (SELECT MAX(b.amount_cents) FROM bids b WHERE b.auction_item_id=i.id) AS high_amount,
         (SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count
  FROM auction_items i JOIN auctions a ON a.id=i.auction_id
  WHERE i.status='published' AND a.status IN ('scheduled','open')
  ORDER BY i.id"
)->fetchAll();
$items=[];
foreach($rows as $row){
 $high=$row['high_amount']===null?null:(int)$row['high_amount'];
 $items[]=[
  'id'=>(int)$row['id'],
  'high_cents'=>$high,
  'minimum_cents'=>$high===null?max(1,(int)$row['opening_price_cents']):$high+(int)$row['minimum_increment_cents'],
  'bid_count'=>(int)$row['bid_count']
 ];
}
echo json_encode(['items'=>$items,'server_time'=>(new DateTimeImmutable('now'))->format('Y-m-d H:i:s')],JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR);
