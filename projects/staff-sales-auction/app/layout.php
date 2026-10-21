<?php
declare(strict_types=1);
function render_page_start(string $title,string $activeNav,array $user):void{
 $admin=($user['role']??'')==='admin';
 $auditor=($user['role']??'')==='auditor';
 $canApprove=has_permission($user,'approver');
 $canManageCategories=has_permission($user,'procurement')||has_permission($user,'custodian');
 $canProcure=$canManageCategories||has_permission($user,'power_user');
 $canReviewApprovals=$canManageCategories||$canApprove;
 ?><!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title><?= e($title) ?> · Staff Sales</title><link rel="stylesheet" href="<?= e(app_url('assets/css/app.css')) ?>"></head><body class="app-body">
 <header class="app-navbar"><a class="brand" href="<?= e(app_url('index.php')) ?>"><span class="brand-mark">SS</span><span><strong>Staff Sales</strong><small>Internal auction portal</small></span></a>
 <nav class="primary-nav" aria-label="Main navigation">
 <a class="nav-link<?= $activeNav==='dashboard'?' is-active':'' ?>" href="<?= e(app_url('index.php')) ?>"<?= $activeNav==='dashboard'?' aria-current="page"':'' ?>>Dashboard</a>
 <a class="nav-link<?= $activeNav==='catalogue'?' is-active':'' ?>" href="<?= e(app_url('auctions.php')) ?>"<?= $activeNav==='catalogue'?' aria-current="page"':'' ?>>Staff catalogue</a>
 <details class="nav-group" name="staff-sales-nav-group"><summary class="nav-link">My activity</summary><div class="nav-group-menu">
 <a class="nav-link<?= $activeNav==='my-bids'?' is-active':'' ?>" href="<?= e(app_url('my-bids.php')) ?>">My bids</a>
 <a class="nav-link<?= $activeNav==='my-results'?' is-active':'' ?>" href="<?= e(app_url('my-results.php')) ?>">My results</a>
 <a class="nav-link<?= $activeNav==='notifications'?' is-active':'' ?>" href="<?= e(app_url('notifications.php')) ?>">Notifications</a>
 </div></details>
 <?php if($admin||$auditor): ?><details class="nav-group" name="staff-sales-nav-group"><summary class="nav-link">Reports & results</summary><div class="nav-group-menu">
 <?php if($admin): ?><a class="nav-link<?= $activeNav==='results'?' is-active':'' ?>" href="<?= e(app_url('admin/results.php')) ?>">Final results</a><?php endif; ?>
 <a class="nav-link<?= $activeNav==='reports'?' is-active':'' ?>" href="<?= e(app_url('admin/reports.php')) ?>">Reports</a>
 <a class="nav-link<?= $activeNav==='audit'?' is-active':'' ?>" href="<?= e(app_url('admin/audit.php')) ?>">Audit log</a>
 </div></details><?php endif; ?>
 <?php if($admin||$canProcure||$canApprove): ?><details class="nav-group" name="staff-sales-nav-group"><summary class="nav-link">Admin settings</summary><div class="nav-group-menu">
 <?php if($admin): ?>
 <a class="nav-link<?= $activeNav==='admin'?' is-active':'' ?>" href="<?= e(app_url('admin/index.php')) ?>">Overview</a>
 <a class="nav-link<?= $activeNav==='auctions'?' is-active':'' ?>" href="<?= e(app_url('admin/auctions.php')) ?>">Auctions</a>
 <a class="nav-link<?= $activeNav==='items'?' is-active':'' ?>" href="<?= e(app_url('admin/items.php')) ?>">Item catalogues</a>
 <a class="nav-link<?= $activeNav==='users'?' is-active':'' ?>" href="<?= e(app_url('admin/users.php')) ?>">User management</a>
 <a class="nav-link<?= $activeNav==='bulk-users'?' is-active':'' ?>" href="<?= e(app_url('admin/bulk-users.php')) ?>">Bulk user upload</a>
 <a class="nav-link<?= $activeNav==='settings'?' is-active':'' ?>" href="<?= e(app_url('admin/settings.php')) ?>">Email settings</a>
 <?php endif; ?>
 <?php if($admin||$canManageCategories): ?><a class="nav-link<?= $activeNav==='categories'?' is-active':'' ?>" href="<?= e(app_url('categories.php')) ?>">Item categories</a><?php endif; ?>
 <?php if($canProcure): ?><a class="nav-link<?= $activeNav==='procurement'?' is-active':'' ?>" href="<?= e(app_url('procurement.php')) ?>">Item changes</a><?php endif; ?><?php if($canManageCategories): ?><a class="nav-link<?= $activeNav==='closeout'?' is-active':'' ?>" href="<?= e(app_url('admin/closeout.php')) ?>">Auction close-out</a><a class="nav-link<?= $activeNav==='pending-items'?' is-active':'' ?>" href="<?= e(app_url('admin/pending-items.php')) ?>">Pending items</a><?php endif; ?>
 <?php if($canReviewApprovals): ?><a class="nav-link<?= $activeNav==='approvals'?' is-active':'' ?>" href="<?= e(app_url('admin/approvals.php')) ?>">Approvals</a><?php endif; ?>
 </div></details><?php endif; ?>
 </nav>
 <div class="navbar-account"><a class="button button-quiet" href="<?= e(app_url('account.php')) ?>">My account</a><span class="user-chip"><strong><?= e($user['display_name']??$user['username']??'Staff') ?></strong><small><?= e(ucfirst((string)($user['role']??'employee'))) ?></small></span><form method="post" action="<?= e(app_url('logout.php')) ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><button class="button button-quiet" type="submit">Sign out</button></form></div></header>
 <main class="page-shell app-main"><div class="mobile-role-label"><?= $admin?'Administrator workspace':($auditor?'Read-only audit access':'Staff workspace') ?></div>
 <?php
}
function render_page_end():void{
 ?><footer class="page-footer">Staff Sales Auction Portal · internal development release</footer></main></body></html><?php
}
