<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
$auctions=(int)db()->query("SELECT COUNT(*) FROM auctions WHERE status IN ('scheduled','open')")->fetchColumn();
$q=db()->prepare('SELECT COUNT(*) FROM bids WHERE bidder_user_id=:id');$q->execute(['id'=>$user['id']]);$bids=(int)$q->fetchColumn();
// Keep this predicate aligned with the Staff Catalogue's visible-lot query.
$catalogueItems=(int)db()->query("SELECT COUNT(*) FROM auction_items i JOIN auctions a ON a.id=i.auction_id WHERE i.status='published' AND a.status IN ('scheduled','open')")->fetchColumn();
$openAuctions=(int)db()->query("SELECT COUNT(*) FROM auctions WHERE status='open' AND starts_at<=datetime('now','localtime') AND ends_at>datetime('now','localtime')")->fetchColumn();
?>
<?php require dirname(__DIR__).'/app/layout.php'; render_page_start('Dashboard','dashboard',$user); ?>
<section class="welcome"><div><p class="eyebrow">STAFF SALES · YOUR PORTAL</p><h1>Good to see you, <?= e($user['display_name']) ?>.</h1><p class="muted">Explore staff sale items and follow your auction activity.</p></div><div class="welcome-icon">↗</div></section>
<section class="stat-grid"><article class="stat-card"><span class="stat-label">Scheduled or open auctions</span><strong><?= $auctions ?></strong><span class="stat-foot"><?= $openAuctions ?> currently open for bidding</span></article><article class="stat-card"><span class="stat-label">Catalogue items</span><strong><?= $catalogueItems ?></strong><span class="stat-foot">Published lots visible in the staff catalogue</span></article><article class="stat-card"><span class="stat-label">Your accepted bids</span><strong><?= $bids ?></strong><span class="stat-foot">Across all lots</span></article></section>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">STAFF SALES</p><h2>Browse the catalogue</h2></div></div><p class="muted">View published items, filter by auction activity and category, and review your own bids.</p><div class="dashboard-actions"><a class="button button-primary" href="<?= e(app_url('auctions.php')) ?>">Browse staff sale items →</a><a class="button button-quiet" href="<?= e(app_url('my-bids.php')) ?>">View my bids</a></div></section>
<section class="section-heading dashboard-next"><div><p class="eyebrow">AUCTION ACTIVITY</p><h2>Your activity and results</h2><p class="muted">Your bid history and finalised outcomes are available from the navigation above.</p></div></section>
<section class="feature-grid dashboard-feature-grid"><article class="feature-card"><span class="feature-icon">⌁</span><h3>Live bid updates</h3><p>Current highest bids and minimums refresh automatically.</p><span class="status-pill">Available</span></article><article class="feature-card"><span class="feature-icon">◷</span><h3>Fair closing times</h3><p>A valid bid in the final two minutes extends the deadline to two minutes after acceptance.</p><span class="status-pill">Enabled</span></article><article class="feature-card"><span class="feature-icon">✓</span><h3>Traceable results</h3><p>Accepted bids and important administrative actions are recorded for review.</p><span class="status-pill">Available</span></article></section>
<?php render_page_end(); ?>
