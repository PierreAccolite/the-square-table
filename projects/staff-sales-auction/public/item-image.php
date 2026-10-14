<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
$id=filter_input(INPUT_GET,'id',FILTER_VALIDATE_INT);if(!$id){http_response_code(404);exit('Image not found.');}
$q=db()->prepare("SELECT i.image_path,i.status,a.status AS auction_status FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.id=:id");$q->execute(['id'=>$id]);$item=$q->fetch();
if(!$item||empty($item['image_path'])||(($user['role']??'')!=='admin'&&($item['status']!=='published'||!in_array($item['auction_status'],['scheduled','open'],true)))){http_response_code(404);exit('Image not found.');}
$base=realpath(dirname(__DIR__).'/storage/item-images');$path=$base ? realpath($base.'/'.basename((string)$item['image_path'])) : false;
if(!$base||!$path||!str_starts_with($path,$base.DIRECTORY_SEPARATOR)||!is_file($path)){http_response_code(404);exit('Image not found.');}
$mime=(new finfo(FILEINFO_MIME_TYPE))->file($path);if(!in_array($mime,['image/jpeg','image/png','image/webp'],true)){http_response_code(404);exit('Image not found.');}
header('Content-Type: '.$mime);header('X-Content-Type-Options: nosniff');header('Cache-Control: private, max-age=300');header('Content-Length: '.filesize($path));readfile($path);
