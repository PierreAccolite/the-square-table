<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
if($_SERVER['REQUEST_METHOD']!=='POST'){http_response_code(405);header('Allow: POST');exit('Method not allowed.');}
if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
$user=current_user(); if($user!==null)audit_log((int)$user['id'],'auth.logout','user',(string)$user['id']);
$_SESSION=[];
if(ini_get('session.use_cookies')){$p=session_get_cookie_params();setcookie(session_name(),'', ['expires'=>time()-42000,'path'=>$p['path'],'domain'=>$p['domain'],'secure'=>$p['secure'],'httponly'=>$p['httponly'],'samesite'=>'Lax']);}
session_destroy(); redirect_to('login.php');
