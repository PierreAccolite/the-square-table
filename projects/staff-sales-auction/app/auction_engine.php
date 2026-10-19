<?php
declare(strict_types=1);

/** Returns the server's local wall-clock timestamp used by the auction database. */
function auction_now(): string {
    return (new DateTimeImmutable('now'))->format('Y-m-d H:i:s');
}

/** Resolve the current accepted high bid, never trusting browser-supplied prices. */
function auction_high_bid(int $itemId): ?array {
    $q = db()->prepare(
        'SELECT b.id,b.bidder_user_id,b.amount_cents,b.placed_at
         FROM bids b WHERE b.auction_item_id=:item
         ORDER BY b.amount_cents DESC,b.placed_at ASC,b.id ASC LIMIT 1'
    );
    $q->execute(['item'=>$itemId]);
    $row=$q->fetch();
    return $row ?: null;
}

/**
 * Finalise every expired scheduled/open auction using a write lock.
 * Results are unique per item, so repeated cron runs cannot create duplicate winners.
 * Returns result rows whose winner email should be attempted.
 */
function finalise_expired_auctions(): array {
    $pdo=db();
    $now=auction_now();
    $emailJobs=[];
    $pdo->exec('BEGIN IMMEDIATE');
    try {
        $q=$pdo->prepare("SELECT id,title FROM auctions WHERE status IN ('scheduled','open') AND ends_at IS NOT NULL AND ends_at<=:now ORDER BY id");
        $q->execute(['now'=>$now]);
        $expired=$q->fetchAll();
        foreach($expired as $auction) {
            $items=$pdo->prepare("SELECT id,title,lot_number,opening_price_cents FROM auction_items WHERE auction_id=:auction AND status='published' ORDER BY id");
            $items->execute(['auction'=>(int)$auction['id']]);
            foreach($items->fetchAll() as $item) {
                $bid=$pdo->prepare('SELECT id,bidder_user_id,amount_cents FROM bids WHERE auction_item_id=:item ORDER BY amount_cents DESC,placed_at ASC,id ASC LIMIT 1');
                $bid->execute(['item'=>(int)$item['id']]);
                $winner=$bid->fetch() ?: null;
                $status=$winner ? 'sold' : 'unsold';
                $insert=$pdo->prepare(
                    "INSERT OR IGNORE INTO auction_results
                     (auction_item_id,auction_id,result_status,winning_bid_id,winner_user_id,final_price_cents,email_status,finalized_at)
                     VALUES (:item,:auction,:status,:bid,:winner,:price,:email,:now)"
                );
                $insert->execute([
                    'item'=>(int)$item['id'],'auction'=>(int)$auction['id'],'status'=>$status,
                    'bid'=>$winner ? (int)$winner['id'] : null,
                    'winner'=>$winner ? (int)$winner['bidder_user_id'] : null,
                    'price'=>$winner ? (int)$winner['amount_cents'] : 0,
                    'email'=>$winner ? 'pending' : 'not_applicable','now'=>$now
                ]);
                if($winner && $insert->rowCount()===1) {
                    $notice=$pdo->prepare("INSERT INTO notifications(user_id,notification_type,title,message,created_at) VALUES(:uid,'auction_result',:title,:message,:now)");
                    $notice->execute([
                        'uid'=>(int)$winner['bidder_user_id'],
                        'title'=>'Auction result: '.$item['title'],
                        'message'=>'You won Lot '.$item['lot_number'].' in '.$auction['title'].' for N$ '.number_format((int)$winner['amount_cents']/100,2,'.',',').'.',
                        'now'=>$now
                    ]);
                }
            }
            $close=$pdo->prepare("UPDATE auctions SET status='closed',closed_at=COALESCE(closed_at,:now),updated_at=:now WHERE id=:id AND status IN ('scheduled','open')");
            $close->execute(['now'=>$now,'id'=>(int)$auction['id']]);
            if($close->rowCount()>0) {
                $actor=(int)(current_user()['id']??0) ?: null;
                $event=$pdo->prepare("INSERT INTO audit_events(actor_user_id,event_type,entity_type,entity_id,details_json,created_at) VALUES(:actor,'auction.finalised','auction',:id,:details,:now)");
                $event->execute(['actor'=>$actor,'id'=>(string)$auction['id'],'details'=>json_encode(['title'=>$auction['title'],'finalized_at'=>$now],JSON_UNESCAPED_SLASHES|JSON_THROW_ON_ERROR),'now'=>$now]);
            }
        }
        $pdo->exec('COMMIT');
    } catch(Throwable $e) {
        try{$pdo->exec('ROLLBACK');}catch(Throwable $ignored){}
        throw $e;
    }

    // Email happens after the results transaction; failures cannot undo final results.
    $jobs=$pdo->query(
        "SELECT r.id,r.final_price_cents,i.title AS item_title,i.lot_number,a.title AS auction_title,
                u.id AS user_id,u.display_name,u.email
         FROM auction_results r
         JOIN auction_items i ON i.id=r.auction_item_id
         JOIN auctions a ON a.id=r.auction_id
         JOIN users u ON u.id=r.winner_user_id
         WHERE r.email_status IN ('pending','failed') AND r.result_status='sold'
         ORDER BY r.id LIMIT 100"
    )->fetchAll();
    foreach($jobs as $job) {
        if(!filter_var((string)($job['email']??''),FILTER_VALIDATE_EMAIL)) {
            $pdo->prepare("UPDATE auction_results SET email_status='no_email',email_last_error='Winner has no valid email address' WHERE id=:id AND email_status IN ('pending','failed')")->execute(['id'=>(int)$job['id']]);
            continue;
        }
        $subject='Staff Sales Auction result: '.(string)$job['item_title'];
        $body="Hello ".(string)$job['display_name'].",\n\nYou are recorded as the winning bidder for ".(string)$job['item_title']." (Lot ".(string)$job['lot_number'].") in ".(string)$job['auction_title'].".\nWinning amount: N$ ".number_format((int)$job['final_price_cents']/100,2,'.',',')."\n\nPlease follow the collection/payment instructions provided by the staff sales team. This message is a result notification, not a payment receipt.\n";
        try {
            $sent=send_app_email((string)$job['email'],$subject,$body);
            $pdo->prepare("UPDATE auction_results SET email_status=:status,email_sent_at=CASE WHEN :sent=1 THEN :now ELSE email_sent_at END,email_last_error=:error WHERE id=:id")
                ->execute(['status'=>$sent?'sent':'failed','sent'=>$sent?1:0,'now'=>auction_now(),'error'=>$sent?null:'Mail transport did not confirm acceptance','id'=>(int)$job['id']]);
        } catch(Throwable $e) {
            error_log('Auction winner notification failed for result '.(int)$job['id'].': '.$e->getMessage());
            $pdo->prepare("UPDATE auction_results SET email_status='failed',email_last_error='Notification attempt failed; see server log' WHERE id=:id")->execute(['id'=>(int)$job['id']]);
        }
    }
    return $emailJobs;
}
