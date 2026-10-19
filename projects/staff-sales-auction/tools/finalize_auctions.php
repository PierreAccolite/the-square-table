#!/usr/bin/env php
<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require dirname(__DIR__).'/app/auction_engine.php';
if(PHP_SAPI!=='cli'){http_response_code(403);exit("CLI only.\n");}
try {
    finalise_expired_auctions();
    $closed=(int)db()->query("SELECT COUNT(*) FROM auctions WHERE status='closed'")->fetchColumn();
    $pending=(int)db()->query("SELECT COUNT(*) FROM auction_results WHERE email_status IN ('pending','failed')")->fetchColumn();
    $noEmail=(int)db()->query("SELECT COUNT(*) FROM auction_results WHERE email_status='no_email'")->fetchColumn();
    echo 'Finalisation check completed at '.auction_now().PHP_EOL;
    echo 'Closed auctions in database: '.$closed.PHP_EOL;
    echo 'Winner notifications awaiting/requiring retry: '.$pending.PHP_EOL;
    echo 'Winner records without a valid email address: '.$noEmail.PHP_EOL;
} catch(Throwable $e) {
    error_log('Auction finalisation failed: '.$e->getMessage());
    fwrite(STDERR,"Auction finalisation failed. Check the PHP/server error log.\n");
    exit(1);
}
