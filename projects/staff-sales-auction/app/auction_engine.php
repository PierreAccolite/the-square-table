<?php
declare(strict_types=1);

/**
 * Transactional bid processing and auction finalisation.
 * All timestamps are local server time, configured by app/bootstrap.php.
 */
function auction_engine_now(): string {
    return date('Y-m-d H:i:s');
}

function auction_open_due(PDO $pdo): int {
    $now = auction_engine_now();
    $q = $pdo->prepare("UPDATE auctions SET status='open',updated_at=:now WHERE status='scheduled' AND starts_at IS NOT NULL AND starts_at<=:now AND ends_at IS NOT NULL AND ends_at>:now");
    $q->execute(['now'=>$now]);
    $count = $q->rowCount();
    if ($count > 0) {
        audit_log(null, 'auction.auto_opened', 'auction', null, ['count'=>$count,'at'=>$now]);
    }
    return $count;
}

/** Close expired auctions and persist exactly one result for every published item. */
function auction_close_expired(PDO $pdo): int {
    auction_open_due($pdo);
    $now = auction_engine_now();
    $q = $pdo->prepare("SELECT id,title FROM auctions WHERE status IN ('scheduled','open') AND ends_at IS NOT NULL AND ends_at<=:now ORDER BY ends_at,id");
    $q->execute(['now'=>$now]);
    $expired = $q->fetchAll();
    $closed = 0;

    foreach ($expired as $candidate) {
        $pdo->exec('BEGIN IMMEDIATE');
        try {
            $check = $pdo->prepare("SELECT id,title,status,ends_at FROM auctions WHERE id=:id");
            $check->execute(['id'=>(int)$candidate['id']]);
            $auction = $check->fetch();
            if (!$auction || !in_array($auction['status'], ['scheduled','open'], true) || empty($auction['ends_at']) || $auction['ends_at'] > $now) {
                $pdo->exec('COMMIT');
                continue;
            }

            $items = $pdo->prepare("SELECT id FROM auction_items WHERE auction_id=:auction AND status='published' ORDER BY id");
            $items->execute(['auction'=>(int)$auction['id']]);
            foreach ($items->fetchAll(PDO::FETCH_COLUMN) as $itemId) {
                $itemId = (int)$itemId;
                $existing = $pdo->prepare('SELECT id FROM auction_item_results WHERE auction_item_id=:item');
                $existing->execute(['item'=>$itemId]);
                if ($existing->fetchColumn()) {
                    continue;
                }

                $winner = $pdo->prepare('SELECT id,bidder_user_id,amount_cents FROM bids WHERE auction_item_id=:item ORDER BY amount_cents DESC,placed_at ASC,id ASC LIMIT 1');
                $winner->execute(['item'=>$itemId]);
                $bid = $winner->fetch();
                if ($bid) {
                    $insert = $pdo->prepare("INSERT INTO auction_item_results(auction_id,auction_item_id,result_status,winning_bid_id,winner_user_id,final_price_cents,finalised_at) VALUES(:auction,:item,'sold',:bid,:winner,:price,:now)");
                    $insert->execute(['auction'=>(int)$auction['id'],'item'=>$itemId,'bid'=>(int)$bid['id'],'winner'=>(int)$bid['bidder_user_id'],'price'=>(int)$bid['amount_cents'],'now'=>$now]);
                    $updateItem = $pdo->prepare("UPDATE auction_items SET status='awarded',updated_at=:now WHERE id=:id AND status='published'");
                    $updateItem->execute(['now'=>$now,'id'=>$itemId]);
                } else {
                    $insert = $pdo->prepare("INSERT INTO auction_item_results(auction_id,auction_item_id,result_status,finalised_at) VALUES(:auction,:item,'unsold',:now)");
                    $insert->execute(['auction'=>(int)$auction['id'],'item'=>$itemId,'now'=>$now]);
                }
            }

            $update = $pdo->prepare("UPDATE auctions SET status='closed',closed_at=:now,updated_at=:now WHERE id=:id AND status IN ('scheduled','open')");
            $update->execute(['now'=>$now,'id'=>(int)$auction['id']]);
            if ($update->rowCount() === 1) {
                audit_log(null, 'auction.closed', 'auction', (string)$auction['id'], ['title'=>$auction['title'],'finalised_items'=>(int)$items->rowCount(),'closed_at'=>$now]);
                $closed++;
            }
            $pdo->exec('COMMIT');
        } catch (Throwable $e) {
            if ($pdo->inTransaction()) {
                $pdo->exec('ROLLBACK');
            }
            throw $e;
        }
    }
    return $closed;
}

/** Place one bid; the database write and anti-sniping extension are atomic. */
function auction_place_bid(PDO $pdo, int $itemId, int $userId, int $amountCents, string $requestId): array {
    if ($amountCents < 1) {
        throw new DomainException('Enter a bid greater than zero.');
    }
    if (!preg_match('/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i', $requestId)) {
        throw new DomainException('This bid form has expired. Reload the page and try again.');
    }

    $pdo->exec('BEGIN IMMEDIATE');
    try {
        $now = auction_engine_now();
        $q = $pdo->prepare("SELECT i.id,i.auction_id,i.lot_number,i.title,i.opening_price_cents,i.minimum_increment_cents,i.status AS item_status,a.title AS auction_title,a.status AS auction_status,a.starts_at,a.ends_at,a.anti_sniping_window_seconds,a.anti_sniping_extension_seconds,u.status AS user_status FROM auction_items i JOIN auctions a ON a.id=i.auction_id JOIN users u ON u.id=:user WHERE i.id=:item");
        $q->execute(['user'=>$userId,'item'=>$itemId]);
        $item = $q->fetch();
        if (!$item) {
            throw new DomainException('That item could not be found.');
        }
        if ($item['user_status'] !== 'active') {
            throw new DomainException('Your account is not active, so you cannot bid.');
        }
        if ($item['item_status'] !== 'published') {
            throw new DomainException('This item is no longer available for bidding.');
        }
        if (!in_array($item['auction_status'], ['scheduled','open'], true)) {
            throw new DomainException('This auction is not accepting bids.');
        }
        if (empty($item['starts_at']) || $item['starts_at'] > $now) {
            throw new DomainException('Bidding has not opened yet.');
        }
        if (empty($item['ends_at']) || $item['ends_at'] <= $now) {
            throw new DomainException('Bidding has closed for this item.');
        }

        if ($item['auction_status'] === 'scheduled') {
            $open = $pdo->prepare("UPDATE auctions SET status='open',updated_at=:now WHERE id=:id AND status='scheduled'");
            $open->execute(['now'=>$now,'id'=>(int)$item['auction_id']]);
        }

        $high = $pdo->prepare('SELECT amount_cents FROM bids WHERE auction_item_id=:item ORDER BY amount_cents DESC,placed_at DESC,id DESC LIMIT 1');
        $high->execute(['item'=>$itemId]);
        $highAmount = $high->fetchColumn();
        $minimum = $highAmount === false
            ? (int)$item['opening_price_cents']
            : (int)$highAmount + (int)$item['minimum_increment_cents'];
        if ($amountCents < $minimum) {
            throw new DomainException('Your bid is too low. The minimum acceptable bid is N$ '.number_format($minimum/100,2,'.',',').'.');
        }

        $insert = $pdo->prepare('INSERT INTO bids(auction_item_id,bidder_user_id,amount_cents,client_request_id,placed_at) VALUES(:item,:user,:amount,:request,:now)');
        $insert->execute(['item'=>$itemId,'user'=>$userId,'amount'=>$amountCents,'request'=>$requestId,'now'=>date('Y-m-d H:i:s.u')]);
        $bidId = (int)$pdo->lastInsertId();

        $endsAt = (string)$item['ends_at'];
        $remaining = strtotime($endsAt) - strtotime($now);
        $window = (int)$item['anti_sniping_window_seconds'];
        $extension = (int)$item['anti_sniping_extension_seconds'];
        if ($remaining <= $window) {
            $endsAt = date('Y-m-d H:i:s', strtotime($now) + $extension);
            $extend = $pdo->prepare('UPDATE auctions SET ends_at=:ends,updated_at=:now WHERE id=:id AND status IN (\'scheduled\',\'open\')');
            $extend->execute(['ends'=>$endsAt,'now'=>$now,'id'=>(int)$item['auction_id']]);
        }

        audit_log($userId, 'bid.accepted', 'bid', (string)$bidId, [
            'auction_id'=>(int)$item['auction_id'],'auction_item_id'=>$itemId,
            'amount_cents'=>$amountCents,'anti_sniping_extended'=>($remaining <= $window),
            'new_ends_at'=>$endsAt
        ]);
        $pdo->exec('COMMIT');
        return ['bid_id'=>$bidId,'ends_at'=>$endsAt,'extended'=>($remaining <= $window),'minimum_next_bid'=>$amountCents+(int)$item['minimum_increment_cents']];
    } catch (Throwable $e) {
        if ($pdo->inTransaction()) {
            $pdo->exec('ROLLBACK');
        }
        throw $e;
    }
}
