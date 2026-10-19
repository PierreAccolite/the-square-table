<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require_login();
header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store, no-cache, must-revalidate, max-age=0');
header('X-Content-Type-Options: nosniff');
$rows=db()->query(
 "SELECT i.id,i.auction_id,a.status AS auction_status,a.starts_at,a.ends_at,i.opening_price_cents,i.minimum_increment_cents,
         (SELECT MAX(b.amount_cents) FROM bids b WHERE b.auction_item_id=i.id) AS high_amount,
         (SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count
  FROM auction_items i JOIN auctions a ON a.id=i.auction_id
  WHERE i.status='published' AND a.status IN ('scheduled','open','closed','cancelled')
  ORDER BY i.id"
)->fetchAll();
$items=[];
$serverNow=(new DateTimeImmutable('now'))->format('Y-m-d H:i:s');
foreach($rows as $row){
 $high=$row['high_amount']===null?null:(int)$row['high_amount'];
 $items[]=[
  'id'=>(int)$row['id'],
  'auction_id'=>(int)$row['auction_id'],
  'auction_status'=>(string)$row['auction_status'],
  'auction_can_bid'=>in_array($row['auction_status'],['scheduled','open'],true)&&!empty($row['starts_at'])&&!empty($row['ends_at'])&&(string)$row['starts_at']<=$serverNow&&(string)$row['ends_at']>$serverNow,
  'auction_display_status'=>$row['auction_status']==='closed'?'Closed':($row['auction_status']==='cancelled'?'Cancelled':((!empty($row['starts_at'])&&(string)$row['starts_at']>$serverNow)?'Scheduled':((!empty($row['ends_at'])&&(string)$row['ends_at']<=$serverNow)?'Bidding closed':'Bidding open'))),
  'high_cents'=>$high,
  'minimum_cents'=>$high===null?max(1,(int)$row['opening_price_cents']):$high+(int)$row['minimum_increment_cents'],
  'bid_count'=>(int)$row['bid_count']
 ];
}
echo json_encode(['items'=>$items,'server_time'=>$serverNow],JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR);
