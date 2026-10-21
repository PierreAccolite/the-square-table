<?php
declare(strict_types=1);

/**
 * Disposable SQLite integration test for the v0.7 finalisation engine.
 * Never overwrites an existing local.php or opens the user's configured database.
 */
$root=dirname(__DIR__);
$configPath=$root.'/config/local.php';
if(is_file($configPath)){
    fwrite(STDERR,"Refusing to run: config/local.php already exists. Run this only in a clean CI checkout.\n");
    exit(2);
}
$dbPath=sys_get_temp_dir().'/staff-sales-finalise-'.bin2hex(random_bytes(8)).'.sqlite';
$config=[
    'app_name'=>'Staff Sales Auction Portal test',
    'app_env'=>'development',
    'base_path'=>'',
    'timezone'=>'UTC',
    'session_secure'=>false,
    'mail_from'=>'no-reply@example.invalid',
    'db'=>['driver'=>'sqlite','path'=>$dbPath,'busy_timeout_ms'=>5000],
];
$cleanup=static function()use($configPath,$dbPath):void{
    if(is_file($configPath))@unlink($configPath);
    foreach([$dbPath,$dbPath.'-journal',$dbPath.'-wal',$dbPath.'-shm'] as $path){if(is_file($path))@unlink($path);}
};
$assert=static function(bool $condition,string $message):void{
    if(!$condition)throw new RuntimeException($message);
};
try{
    if(file_put_contents($configPath,"<?php\ndeclare(strict_types=1);\nreturn ".var_export($config,true).";\n")===false)throw new RuntimeException('Could not create temporary test config.');
    $schemaPdo=new PDO('sqlite:'.$dbPath,null,null,[PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION]);
    $schema=file_get_contents($root.'/database/migrations/001_initial_schema.sql');
    if($schema===false)throw new RuntimeException('Initial schema file is missing.');
    $schemaPdo->exec($schema);
    unset($schemaPdo);

    $command=escapeshellarg(PHP_BINARY).' '.escapeshellarg($root.'/tools/apply_migrations.php');
    exec($command.' 2>&1',$migrationOutput,$migrationCode);
    $assert($migrationCode===0,'Migration script failed: '.implode("\n",$migrationOutput));

    require $root.'/app/bootstrap.php';
    require $root.'/app/auction_engine.php';
    $pdo=db();
    $userInsert=$pdo->prepare("INSERT INTO users(username,display_name,password_hash,role,status) VALUES(:username,:name,:hash,'employee','active')");
    $userInsert->execute(['username'=>'testadmin','name'=>'Test Admin','hash'=>password_hash('TestPassword-12345',PASSWORD_DEFAULT)]);
    $adminId=(int)$pdo->lastInsertId();
    $userInsert->execute(['username'=>'winner','name'=>'Winning Bidder','hash'=>password_hash('TestPassword-12345',PASSWORD_DEFAULT)]);
    $winnerId=(int)$pdo->lastInsertId();
    $userInsert->execute(['username'=>'otherbidder','name'=>'Other Bidder','hash'=>password_hash('TestPassword-12345',PASSWORD_DEFAULT)]);
    $otherId=(int)$pdo->lastInsertId();
    save_user_permissions($otherId,['power_user'], $adminId);
    $assert(in_array('power_user',user_permissions($otherId),true),'Power-user permission was not saved by the migrated schema.');
    $approvalColumns=array_column($pdo->query('PRAGMA table_info(approval_requests)')->fetchAll(),'name');
    $assert(in_array('approval_stage',$approvalColumns,true)&&in_array('procurement_reviewed_by',$approvalColumns,true)&&in_array('procurement_review_comment',$approvalColumns,true),'Staged approval migration is incomplete.');
    $mailSettingTable=(int)$pdo->query("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='app_settings'")->fetchColumn();
    $assert($mailSettingTable===1,'Mail settings table was not created by migrations.');

    $now=new DateTimeImmutable('now');
    $start=$now->modify('-2 hours')->format('Y-m-d H:i:s');
    $end=$now->modify('-1 hour')->format('Y-m-d H:i:s');
    $q=$pdo->prepare("INSERT INTO auctions(title,status,starts_at,ends_at,created_by) VALUES('Integration test auction','open',:start,:end,:actor)");
    $q->execute(['start'=>$start,'end'=>$end,'actor'=>$adminId]);
    $auctionId=(int)$pdo->lastInsertId();
    $item=$pdo->prepare("INSERT INTO auction_items(auction_id,lot_number,title,opening_price_cents,minimum_increment_cents,status) VALUES(:auction,:lot,:title,1000,100,'published')");
    $item->execute(['auction'=>$auctionId,'lot'=>'1','title'=>'Item with bids']);
    $soldItem=(int)$pdo->lastInsertId();
    $item->execute(['auction'=>$auctionId,'lot'=>'2','title'=>'Item without bids']);
    $unsoldItem=(int)$pdo->lastInsertId();
    $bid=$pdo->prepare('INSERT INTO bids(auction_item_id,bidder_user_id,amount_cents,client_request_id,placed_at) VALUES(:item,:user,:amount,:request,:placed)');
    $bid->execute(['item'=>$soldItem,'user'=>$otherId,'amount'=>1500,'request'=>'11111111-1111-4111-8111-111111111111','placed'=>$now->modify('-90 minutes')->format('Y-m-d H:i:s.u')]);
    $bid->execute(['item'=>$soldItem,'user'=>$winnerId,'amount'=>2000,'request'=>'22222222-2222-4222-8222-222222222222','placed'=>$now->modify('-80 minutes')->format('Y-m-d H:i:s.u')]);

    finalise_expired_auctions();
    $q=$pdo->prepare('SELECT result_status,winner_user_id,final_price_cents,email_status FROM auction_results WHERE auction_item_id=:item');
    $q->execute(['item'=>$soldItem]);$sold=$q->fetch();
    $assert($sold!==false,'Sold item result was not created.');
    $assert($sold['result_status']==='sold','Highest-bid item was not marked sold.');
    $assert((int)$sold['winner_user_id']===$winnerId,'The highest bidder was not selected as winner.');
    $assert((int)$sold['final_price_cents']===2000,'Final price does not match the highest accepted bid.');
    $statusQ=$pdo->prepare('SELECT status FROM auction_items WHERE id=:item');$statusQ->execute(['item'=>$soldItem]);
    $assert((string)$statusQ->fetchColumn()==='awarded','Winning item was not locked as awarded.');
    $assert($sold['email_status']==='no_email','Missing winner email was not recorded.');
    $q->execute(['item'=>$unsoldItem]);$unsold=$q->fetch();
    $assert($unsold!==false&&$unsold['result_status']==='unsold','No-bid item was not marked unsold.');
    $assert((string)$pdo->query('SELECT status FROM auctions WHERE id='.(int)$auctionId)->fetchColumn()==='closed','Expired auction was not closed.');
    $assert((int)$pdo->query("SELECT COUNT(*) FROM notifications WHERE user_id=".(int)$winnerId." AND notification_type='auction_result'")->fetchColumn()===1,'Winner notification was not created exactly once.');
    finalise_expired_auctions();
    $assert((int)$pdo->query('SELECT COUNT(*) FROM auction_results WHERE auction_id='.(int)$auctionId)->fetchColumn()===2,'Repeated finalisation duplicated or lost results.');
    $assert((int)$pdo->query("SELECT COUNT(*) FROM notifications WHERE user_id=".(int)$winnerId." AND notification_type='auction_result'")->fetchColumn()===1,'Repeated finalisation duplicated the winner notification.');
    echo "PASS: expired auction closes, highest bid wins, no-bid item is unsold, missing email is tracked, and finalisation is idempotent.\n";
}catch(Throwable $e){
    fwrite(STDERR,"FAIL: ".$e->getMessage()."\n");
    exit(1);
}finally{
    $cleanup();
}
