#!/usr/bin/env php
<?php
declare(strict_types=1);
if (PHP_SAPI !== 'cli') {
    http_response_code(404);
    exit;
}
require dirname(__DIR__).'/app/bootstrap.php';
require dirname(__DIR__).'/app/auction_engine.php';
try {
    $pdo = db();
    $opened = auction_open_due($pdo);
    $closed = auction_close_expired($pdo);
    printf("Auction scheduler finished. Opened: %d; closed/finalised: %d. Time: %s\n", $opened, $closed, auction_engine_now());
} catch (Throwable $e) {
    fwrite(STDERR, "Auction scheduler failed: ".$e->getMessage()."\n");
    exit(1);
}
