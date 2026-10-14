<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
require_role('admin');
header('Content-Type: text/csv; charset=utf-8');
header('Content-Disposition: attachment; filename="staff-sales-user-import-template.csv"');
header('X-Content-Type-Options: nosniff');
$out=fopen('php://output','wb');
fputcsv($out,['username','display_name','employee_number','role','initial_password']);
fputcsv($out,['jdoe','Jane Doe','123456','employee','CHANGE-THIS-12-CHAR']);
fclose($out);
