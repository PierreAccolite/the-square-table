<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
$counts=[
 'users'=>(int)db()->query('SELECT COUNT(*) FROM users')->fetchColumn(),
 'auctions'=>(int)db()->query('SELECT COUNT(*) FROM auctions')->fetchColumn(),
 'bids'=>(int)db()->query('SELECT COUNT(*) FROM bids')->fetchColumn(),
 'audit'=>(int)db()->query('SELECT COUNT(*) FROM audit_events')->fetchColumn()
];
?>
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Administration · Staff Sales</title><link rel="stylesheet" href="<?= e(app_url('assets/css/app.css')) ?>"></head><body>
<header class="topbar"><a class="brand" href="<?= e(app_url('index.php')) ?>"><span class="brand-mark">SS</span><span><strong>Staff Sales</strong><small>Administration</small></span></a><div class="topbar-actions"><a class="text-link" href="<?= e(app_url('index.php')) ?>">Employee portal</a><form method="post" action="<?= e(app_url('logout.php')) ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><button class="button button-quiet" type="submit">Sign out</button></form></div></header>
<main class="page-shell"><section class="welcome"><div><p class="eyebrow">CONTROL CENTRE</p><h1>Administration</h1><p class="muted">Welcome, <?= e($user['display_name']) ?>. The initial data foundation is connected.</p></div><div class="welcome-icon">⚙</div></section>
<div class="notice"><span class="notice-dot"></span><div><strong>Administrator tools are being built in stages</strong><p>v0.1 provides account setup, schema and a read-only overview. Auction editing and employee management screens are not enabled yet.</p></div></div>
<section class="stat-grid"><article class="stat-card"><span class="stat-label">User accounts</span><strong><?= $counts['users'] ?></strong></article><article class="stat-card"><span class="stat-label">Auctions</span><strong><?= $counts['auctions'] ?></strong></article><article class="stat-card"><span class="stat-label">Accepted bids</span><strong><?= $counts['bids'] ?></strong></article><article class="stat-card"><span class="stat-label">Audit events</span><strong><?= $counts['audit'] ?></strong></article></section><p><a class="text-link" href="<?= e(app_url('index.php')) ?>">← Return to employee portal</a></p></main></body></html>
