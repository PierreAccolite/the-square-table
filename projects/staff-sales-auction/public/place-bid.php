<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require dirname(__DIR__).'/app/auction_engine.php';

$user = require_permission('bidder');
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    header('Allow: POST');
    exit('Method not allowed.');
}
if (!verify_csrf(isset($_POST['csrf_token']) ? (string)$_POST['csrf_token'] : null)) {
    http_response_code(400);
    exit('Invalid request token. Reload the page and try again.');
}
$itemId = filter_input(INPUT_POST, 'item_id', FILTER_VALIDATE_INT);
$rawAmount = trim((string)($_POST['amount'] ?? ''));
$requestId = (string)($_POST['client_request_id'] ?? '');
$amountCents = null;
if (preg_match('/^\d{1,9}(?:\.\d{1,2})?$/', $rawAmount)) {
    $parts = explode('.', $rawAmount, 2);
    $amountCents = ((int)$parts[0] * 100) + (int)str_pad($parts[1] ?? '', 2, '0');
}
if (!$itemId || $amountCents === null || $amountCents < 1) {
    redirect_to('auctions.php?bid_error='.rawurlencode('Enter a valid bid amount.').'#item-'.max(0,(int)$itemId));
}
try {
    auction_close_expired(db());
    $result = auction_place_bid(db(), (int)$itemId, (int)$user['id'], $amountCents, $requestId);
    $message = $result['extended']
        ? 'Bid accepted. The auction closing time was extended by two minutes.'
        : 'Your bid was accepted.';
    redirect_to('auctions.php?bid_success='.rawurlencode($message).'#item-'.(int)$itemId);
} catch (DomainException $e) {
    redirect_to('auctions.php?bid_error='.rawurlencode($e->getMessage()).'#item-'.(int)$itemId);
} catch (PDOException $e) {
    if (str_contains(strtolower($e->getMessage()), 'unique') && str_contains(strtolower($e->getMessage()), 'client_request_id')) {
        redirect_to('auctions.php?bid_error='.rawurlencode('This bid was already submitted. Reload the catalogue to see the latest price.').'#item-'.(int)$itemId);
    }
    error_log('Staff Sales bid processing failed: '.$e->getMessage());
    redirect_to('auctions.php?bid_error='.rawurlencode('The bid could not be processed. Please reload and try again.').'#item-'.(int)$itemId);
} catch (Throwable $e) {
    error_log('Staff Sales bid processing failed: '.$e->getMessage());
    redirect_to('auctions.php?bid_error='.rawurlencode('The bid could not be processed. Please contact the portal administrator if this continues.').'#item-'.(int)$itemId);
}
