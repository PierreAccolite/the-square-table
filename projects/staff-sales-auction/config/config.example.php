<?php
declare(strict_types=1);

/* Copy to config/local.php and set local values. Never commit real credentials. */
return [
    'app_name' => 'Staff Sales Auction Portal',
    'app_env' => 'development', // Use 'production' on the internal server.
    'base_path' => '',          // Empty when public/ is the web root.
    'timezone' => 'Africa/Windhoek',
    'session_secure' => true,   // False only for isolated localhost HTTP development.
    'db' => [
        'host' => '127.0.0.1',
        'port' => 3306,
        'database' => 'staff_sales_auction',
        'username' => 'staff_sales_app',
        'password' => 'CHANGE_ME',
        'charset' => 'utf8mb4',
    ],
];
