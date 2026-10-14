<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
function cents_to_money(int $cents):string{return number_format($cents/100,2,'.',',');}
$auctions=db()->query("SELECT id,title,description,starts_at,ends_at,status FROM auctions WHERE status IN ('scheduled','open') ORDER BY starts_at,id")->fetchAll();
$itemsByAuction=[];
if($auctions){
 $ids=array_map(static fn($a)=>(int)$a['id'],$auctions);
 $marks=implode(',',array_fill(0,count($ids),'?'));
 $q=db()->prepare("SELECT id,auction_id,lot_number,title,description,condition_notes,opening_price_cents,minimum_increment_cents FROM auction_items WHERE status='published' AND auction_id IN ($marks) ORDER BY auction_id,CAST(lot_number AS INTEGER),lot_number,id");
 $q->execute($ids);foreach($q->fetchAll() as $item)$itemsByAuction[(int)$item['auction_id']][]=$item;
}
?>
<?php require dirname(__DIR__).'/app/layout.php'; render_page_start('Staff sale catalogue','catalogue',$user); ?><section class="welcome"><div><p class="eyebrow">STAFF SALES · CATALOGUE</p><h1>Browse staff sale items</h1><p class="muted">Explore the published items in scheduled staff sales. Bidding is not open in this development release.</p></div><div class="welcome-icon">▤</div></section>
<?php if(!$auctions): ?><section class="content-panel empty-state"><strong>No staff sales are published yet</strong><p>When an administrator schedules an auction, its published items will appear here.</p></section><?php else: foreach($auctions as $auction): ?><section class="content-panel"><div class="section-heading"><div><p class="eyebrow">STAFF SALE</p><h2><?= e($auction['title']) ?></h2></div><span class="status-pill"><?= e(ucfirst($auction['status'])) ?></span></div><?php if($auction['description']): ?><p class="muted"><?= nl2br(e($auction['description'])) ?></p><?php endif; ?><div class="date-strip"><span><strong>Opens</strong><?= e($auction['starts_at']??'Not set') ?></span><span><strong>Closes</strong><?= e($auction['ends_at']??'Not set') ?></span></div><?php $items=$itemsByAuction[(int)$auction['id']]??[]; if(!$items): ?><div class="empty-state"><p>No published items in this auction yet.</p></div><?php else: ?><div class="catalogue-grid"><?php foreach($items as $item): ?><article class="catalogue-card"><div class="catalogue-lot">LOT <?= e($item['lot_number']) ?></div><h3><?= e($item['title']) ?></h3><?php if($item['description']): ?><p><?= nl2br(e($item['description'])) ?></p><?php endif; ?><?php if($item['condition_notes']): ?><p class="condition-note"><strong>Condition:</strong> <?= nl2br(e($item['condition_notes'])) ?></p><?php endif; ?><div class="price-line"><span>Opening price</span><strong>N$ <?= e(cents_to_money((int)$item['opening_price_cents'])) ?></strong></div><div class="catalogue-meta">Minimum bid increment: N$ <?= e(cents_to_money((int)$item['minimum_increment_cents'])) ?></div><span class="status-pill">Bidding not yet enabled</span></article><?php endforeach; ?></div><?php endif; ?></section><?php endforeach; endif; ?>
<?php render_page_end(); ?>