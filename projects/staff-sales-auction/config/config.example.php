<?php
declare(strict_types=1);
/* Copy to config/local.php. Setup creates the SQLite database locally. */
return [
 'app_name'=>'Staff Sales Auction Portal',
 'app_env'=>'development',
 'base_path'=>'',
 'timezone'=>'Africa/Windhoek',
 'session_secure'=>true,
 'db'=>[
  'driver'=>'sqlite',
  'path'=>dirname(__DIR__).'/storage/database/staff_sales_auction.sqlite',
  'busy_timeout_ms'=>5000,
 ],
];
