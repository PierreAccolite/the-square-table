<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require dirname(__DIR__).'/app/auction_engine.php';

$user=require_permission('bidder');
if($_SERVER['REQUEST_METHOD']!=='POST'){http_response_code(405);header('Allow: POST');exit('POST required.');}
if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token. Reload the catalogue and try again.');}

$itemId=filter_var($_POST['item_id']??null,FILTER_VALIDATE_INT,['options'=>['min_range'=>1]]);
$rawAmount=trim((string)($_POST['amount']??''));
$requestId=trim((string)($_POST['client_request_id']??''));
$back=(int)($_POST['return_auction']??0);
if(!$itemId || !preg_match('/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i',$requestId)){
    http_response_code(400);exit('Invalid bid request. Return to the catalogue and try again.');
}
if(!preg_match('/^(?:0|[1-9][0-9]{0,8})(?:\.[0-9]{1,2})?$/',$rawAmount)){
    $_SESSION['bid_flash']='Enter a valid bid amount using up to two decimal places.';
    redirect_to('auctions.php'.($back>0?'#auction-'.$back:''));
}
$parts=explode('.',$rawAmount,2);$amountCents=((int)$parts[0]*100)+(isset($parts[1])?(int)str_pad($parts[1],2,'0'):0);
$pdo=db();
$now=auction_now();
$error='';
$pdo->exec('BEGIN IMMEDIATE');
try {
    $q=$pdo->prepare("SELECT i.id,i.auction_id,i.lot_number,i.title,i.status AS item_status,i.opening_price_cents,i.minimum_increment_cents,i.buyout_enabled,a.title AS auction_title,a.status AS auction_status,a.starts_at,a.ends_at,a.anti_sniping_window_seconds,a.anti_sniping_extension_seconds FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.id=:id");
    $q->execute(['id'=>(int)$itemId]);$item=$q->fetch();
    if(!$item || $item['item_status']!=='published') $error='This item is no longer available for bidding.';
    elseif(!in_array($item['auction_status'],['scheduled','open'],true)) $error='This auction is not accepting bids.';
    elseif(empty($item['starts_at'])||empty($item['ends_at'])||(string)$item['starts_at']>$now) $error='Bidding has not opened yet.';
    elseif((string)$item['ends_at']<=$now) $error='Bidding has closed for this item.';
    else {
        $dupe=$pdo->prepare('SELECT id FROM bids WHERE client_request_id=:request');$dupe->execute(['request'=>$requestId]);
        if($dupe->fetch()) {
            $error='This bid request was already processed. Refresh the catalogue to see the current price.';
        } else {
            $high=$pdo->prepare('SELECT MAX(amount_cents) FROM bids WHERE auction_item_id=:item');$high->execute(['item'=>(int)$itemId]);$highAmount=$high->fetchColumn();
            $minimum=$highAmount===null?(int)$item['opening_price_cents']:(int)$highAmount+(int)$item['minimum_increment_cents'];
            if($amountCents<$minimum) {
                $error='Your bid is below the current minimum of N$ '.number_format($minimum/100,2,'.',',').'. Refresh the catalogue and try again.';
            } else {
                $insert=$pdo->prepare('INSERT INTO bids(auction_item_id,bidder_user_id,amount_cents,client_request_id,placed_at) VALUES(:item,:user,:amount,:request,:now)');
                $insert->execute(['item'=>(int)$itemId,'user'=>(int)$user['id'],'amount'=>$amountCents,'request'=>$requestId,'now'=>(new DateTimeImmutable('now'))->format('Y-m-d H:i:s.u')]);
                $deadline=new DateTimeImmutable((string)$item['ends_at']);
                $nowDt=new DateTimeImmutable($now);
                $remaining=$deadline->getTimestamp()-$nowDt->getTimestamp();
                if($remaining <= (int)$item['anti_sniping_window_seconds']) {
                    $newEnd=$nowDt->modify('+'.(int)$item['anti_sniping_extension_seconds'].' seconds')->format('Y-m-d H:i:s');
                    $pdo->prepare("UPDATE auctions SET ends_at=:end,status='open',updated_at=:now WHERE id=:id AND status IN ('scheduled','open')")->execute(['end'=>$newEnd,'now'=>$now,'id'=>(int)$item['auction_id']]);
                } else {
                    $pdo->prepare("UPDATE auctions SET status='open',updated_at=:now WHERE id=:id AND status='scheduled'")->execute(['now'=>$now,'id'=>(int)$item['auction_id']]);
                }
                $pdo->prepare("INSERT INTO audit_events(actor_user_id,event_type,entity_type,entity_id,details_json,created_at) VALUES(:actor,'bid.accepted','bid',:id,:details,:now)")
                    ->execute(['actor'=>(int)$user['id'],'id'=>(string)$pdo->lastInsertId(),'details'=>json_encode(['auction_item_id'=>(int)$itemId,'amount_cents'=>$amountCents,'auction_id'=>(int)$item['auction_id']],JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR),'now'=>$now]);
                $pdo->commit();
                $_SESSION['bid_flash']='Bid accepted: N$ '.number_format($amountCents/100,2,'.',',').'.';
                redirect_to('auctions.php#auction-'.(int)$item['auction_id']);
            }
        }
    }
    $pdo->rollBack();
} catch(Throwable $e) {
    if($pdo->inTransaction())$pdo->rollBack();
    error_log('Staff Sales bid submission failed: '.$e->getMessage());
    $error='The bid could not be processed. Please refresh the catalogue and try again.';
}
$_SESSION['bid_flash']=$error;
redirect_to('auctions.php'.($back>0?'#auction-'.$back:''));
