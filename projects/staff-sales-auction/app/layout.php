<?php
declare(strict_types=1);
function render_page_start(string $title,string $activeNav,array $user):void{
 $admin=($user['role']??'')==='admin';
 $auditor=($user['role']??'')==='auditor';
 $nav=[
  ['key'=>'dashboard','label'=>'Dashboard','url'=>'index.php','roles'=>['employee','admin','auditor']],
  ['key'=>'catalogue','label'=>'Staff catalogue','url'=>'auctions.php','roles'=>['employee','admin','auditor']],
  ['key'=>'my-bids','label'=>'My bids','url'=>'my-bids.php','roles'=>['employee','admin']],
  ['key'=>'notifications','label'=>'Notifications','url'=>'notifications.php','roles'=>['employee','admin','auditor']],
 ];
 if($admin){
  $nav[]=['key'=>'admin','label'=>'Overview','url'=>'admin/index.php','roles'=>['admin']];
  $nav[]=['key'=>'auctions','label'=>'Auctions','url'=>'admin/auctions.php','roles'=>['admin']];
  $nav[]=['key'=>'users','label'=>'Users','url'=>'admin/users.php','roles'=>['admin']];
  $nav[]=['key'=>'audit','label'=>'Audit log','url'=>'admin/audit.php','roles'=>['admin']];
 }elseif($auditor){
  $nav[]=['key'=>'audit','label'=>'Audit log','url'=>'admin/audit.php','roles'=>['auditor']];
 }
 ?><!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title><?= e($title) ?> · Staff Sales</title><link rel="stylesheet" href="<?= e(app_url('assets/css/app.css')) ?>"></head><body class="app-body">
 <header class="app-navbar"><a class="brand" href="<?= e(app_url('index.php')) ?>"><span class="brand-mark">SS</span><span><strong>Staff Sales</strong><small>Internal auction portal</small></span></a>
 <nav class="primary-nav" aria-label="Main navigation"><?php foreach($nav as $item): ?><a class="nav-link<?= $activeNav===$item['key']?' is-active':'' ?>" href="<?= e(app_url($item['url'])) ?>"<?= $activeNav===$item['key']?' aria-current="page"':'' ?>><?= e($item['label']) ?></a><?php endforeach; ?></nav>
 <div class="navbar-account"><span class="user-chip"><strong><?= e($user['display_name']??$user['username']??'Staff') ?></strong><small><?= e(ucfirst((string)($user['role']??'employee'))) ?></small></span><form method="post" action="<?= e(app_url('logout.php')) ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><button class="button button-quiet" type="submit">Sign out</button></form></div></header>
 <main class="page-shell app-main"><div class="mobile-role-label"><?= $admin?'Administrator workspace':($auditor?'Read-only audit access':'Staff workspace') ?></div>
 <?php
}
function render_page_end():void{
 ?><footer class="page-footer">Staff Sales Auction Portal · internal development release</footer></main></body></html><?php
}
