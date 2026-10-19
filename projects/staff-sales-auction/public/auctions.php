<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
require dirname(__DIR__).'/app/auction_engine.php';
$user=require_login();
auction_close_expired(db());
function cents_to_money(int $cents):string{return number_format($cents/100,2,'.',',');}
$auctions=db()->query("SELECT id,title,description,starts_at,ends_at,status FROM auctions WHERE status IN ('scheduled','open') ORDER BY starts_at,id")->fetchAll();
$itemsByAuction=[];
if($auctions){
 $ids=array_map(static fn($a)=>(int)$a['id'],$auctions);
 $marks=implode(',',array_fill(0,count($ids),'?'));
 $q=db()->prepare("SELECT i.id,i.auction_id,i.lot_number,i.title,i.description,i.condition_notes,i.image_path,i.opening_price_cents,i.minimum_increment_cents,i.buyout_enabled,i.buyout_price_cents,(SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count,(SELECT MAX(b.amount_cents) FROM bids b WHERE b.auction_item_id=i.id) AS high_bid FROM auction_items i WHERE i.status='published' AND auction_id IN ($marks) ORDER BY auction_id,CAST(lot_number AS INTEGER),lot_number,id");
 $q->execute($ids);foreach($q->fetchAll() as $item)$itemsByAuction[(int)$item['auction_id']][]=$item;
}
$canBid=has_permission($user,'bidder');
?>
<?php require dirname(__DIR__).'/app/layout.php'; render_page_start('Staff sale catalogue','catalogue',$user); ?>
<section class="welcome"><div><p class="eyebrow">STAFF SALES · CATALOGUE</p><h1>Browse staff sale items</h1><p class="muted">Live prices and closing times update automatically while an auction is running.</p></div><div class="welcome-icon">▤</div></section>
<?php if(isset($_GET['bid_success'])): ?><div class="notice" role="status"><span class="notice-dot"></span><div><strong>Bid accepted</strong><p><?= e((string)$_GET['bid_success']) ?></p></div></div><?php endif; ?>
<?php if(isset($_GET['bid_error'])): ?><div class="alert" role="alert"><?= e((string)$_GET['bid_error']) ?></div><?php endif; ?>
<?php if(!$auctions): ?><section class="content-panel empty-state"><strong>No active staff sales</strong><p>There are no scheduled or open auctions at the moment. Check back when the next staff sale is published.</p></section><?php else: foreach($auctions as $auction): ?><section class="content-panel"><div class="section-heading"><div><p class="eyebrow">STAFF SALE</p><h2><?= e($auction['title']) ?></h2></div><span class="status-pill auction-status"><?= e(ucfirst($auction['status'])) ?></span></div><?php if($auction['description']): ?><p class="muted"><?= nl2br(e($auction['description'])) ?></p><?php endif; ?><div class="date-strip"><span><strong>Opens</strong><?= e($auction['starts_at']??'Not set') ?></span><span><strong>Closes</strong><span data-auction-end="<?= e((string)$auction['ends_at']) ?>"><?= e($auction['ends_at']??'Not set') ?></span></span></div><?php $items=$itemsByAuction[(int)$auction['id']]??[]; if(!$items): ?><div class="empty-state"><p>No published items in this auction yet.</p></div><?php else: ?><div class="catalogue-grid"><?php foreach($items as $item): $current=$item['high_bid']===null?(int)$item['opening_price_cents']:(int)$item['high_bid']; $minimum=$item['high_bid']===null?(int)$item['opening_price_cents']:(int)$item['high_bid']+(int)$item['minimum_increment_cents']; ?><article class="catalogue-card" id="item-<?= (int)$item['id'] ?>" data-auction-item="<?= (int)$item['id'] ?>"><div class="catalogue-lot">LOT <?= e($item['lot_number']) ?></div><?php if($item['image_path']): ?><img class="catalogue-image" src="<?= e(app_url('item-image.php?id='.(int)$item['id'])) ?>" alt="<?= e($item['title']) ?>" loading="lazy"><?php else: ?><div class="catalogue-image-placeholder">No image yet</div><?php endif; ?><h3><?= e($item['title']) ?></h3><?php if($item['description']): ?><p><?= nl2br(e($item['description'])) ?></p><?php endif; ?><?php if($item['condition_notes']): ?><p class="condition-note"><strong>Condition:</strong> <?= nl2br(e($item['condition_notes'])) ?></p><?php endif; ?><div class="price-line"><span>Current price</span><strong data-current-price="<?= (int)$item['id'] ?>">N$ <?= e(cents_to_money($current)) ?></strong></div><div class="catalogue-meta">Minimum bid increment: N$ <?= e(cents_to_money((int)$item['minimum_increment_cents'])) ?></div><div class="catalogue-meta"><span data-bid-count="<?= (int)$item['id'] ?>"><?= (int)$item['bid_count'] ?></span> bid(s) recorded</div><div class="catalogue-meta"> <strong>Time remaining:</strong> <span data-countdown="<?= (int)$item['id'] ?>" data-end="<?= e((string)$auction['ends_at']) ?>">Checking…</span></div><?php if((int)($item['buyout_enabled']??0)===1): ?><div class="price-line"><span>Configured buyout price</span><strong>N$ <?= e(cents_to_money((int)$item['buyout_price_cents'])) ?></strong></div><div class="catalogue-meta">Buyout transactions are not enabled.</div><?php endif; ?><?php if($canBid): ?><form method="post" action="<?= e(app_url('place-bid.php')) ?>" class="admin-form bid-form" data-bid-form="<?= (int)$item['id'] ?>"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="item_id" value="<?= (int)$item['id'] ?>"><input type="hidden" name="client_request_id" value="<?php $bytes=random_bytes(16);$bytes[6]=chr((ord($bytes[6])&0x0f)|0x40);$bytes[8]=chr((ord($bytes[8])&0x3f)|0x80);$hex=bin2hex($bytes);echo e(substr($hex,0,8).'-'.substr($hex,8,4).'-'.substr($hex,12,4).'-'.substr($hex,16,4).'-'.substr($hex,20)); ?>"><label for="bid-<?= (int)$item['id'] ?>">Your bid (NAD)</label><input id="bid-<?= (int)$item['id'] ?>" type="number" name="amount" min="<?= e(number_format($minimum/100,2,'.','')) ?>" step="0.01" required inputmode="decimal" placeholder="<?= e(number_format($minimum/100,2,'.','')) ?>" data-bid-amount="<?= (int)$item['id'] ?>"><p class="form-help">Minimum acceptable bid: N$ <span data-minimum-bid="<?= (int)$item['id'] ?>"><?= e(cents_to_money($minimum)) ?></span></p><button class="button button-primary" type="submit" data-bid-button="<?= (int)$item['id'] ?>" disabled>Checking auction…</button><p class="form-help" data-bid-help="<?= (int)$item['id'] ?>">Bid status is being checked.</p></form><?php else: ?><span class="status-pill">View only</span><?php endif; ?><?php if(has_permission($user,'procurement')||has_permission($user,'custodian')): ?><p><a class="text-link" href="<?= e(app_url('request-item-change.php?id='.(int)$item['id'])) ?>">Request item change →</a></p><?php endif; ?></article><?php endforeach; ?></div><?php endif; ?></section><?php endforeach; endif; ?>
<script>
(function(){
 const money = cents => 'N$ ' + (Number(cents)/100).toLocaleString('en-NA',{minimumFractionDigits:2,maximumFractionDigits:2});
 function updateCountdowns(){
  document.querySelectorAll('[data-countdown]').forEach(el=>{
   const end=el.dataset.end;
   if(!end){el.textContent='Not set';return;}
   const ms=Date.parse(end.replace(' ','T'))-Date.now();
   if(ms<=0){el.textContent='Closing / closed';return;}
   const sec=Math.floor(ms/1000), days=Math.floor(sec/86400), hours=Math.floor((sec%86400)/3600), mins=Math.floor((sec%3600)/60), secs=sec%60;
   el.textContent=(days?days+'d ':'')+(hours?hours+'h ':'')+mins+'m '+secs+'s';
  });
 }
 async function refresh(){
  const cards=document.querySelectorAll('[data-auction-item]');
  for(const card of cards){
   const id=card.dataset.auctionItem;
   try{
    const response=await fetch('<?= e(app_url('auction-status.php')) ?>?id='+encodeURIComponent(id),{credentials:'same-origin',cache:'no-store',headers:{'Accept':'application/json'}});
    if(!response.ok)continue;
    const data=await response.json();
    const price=card.querySelector('[data-current-price="'+id+'"]');
    const count=card.querySelector('[data-bid-count="'+id+'"]');
    const min=card.querySelector('[data-minimum-bid="'+id+'"]');
    const amount=card.querySelector('[data-bid-amount="'+id+'"]');
    const button=card.querySelector('[data-bid-button="'+id+'"]');
    const help=card.querySelector('[data-bid-help="'+id+'"]');
    if(price)price.textContent=money(data.current_price_cents);
    if(count)count.textContent=String(data.bid_count);
    if(min)min.textContent=(Number(data.minimum_bid_cents)/100).toLocaleString('en-NA',{minimumFractionDigits:2,maximumFractionDigits:2});
    if(amount){amount.min=(Number(data.minimum_bid_cents)/100).toFixed(2);amount.placeholder=amount.min;}
    const countdown=card.querySelector('[data-countdown="'+id+'"]');
    if(countdown&&data.ends_at)countdown.dataset.end=data.ends_at;
    if(button){
     button.disabled=!data.can_bid;
     button.textContent=data.can_bid?'Place bid':'Bidding closed';
     if(help)help.textContent=data.can_bid?'Your bid is recorded only after server-side validation.':'This item is not currently accepting bids.';
    }
   }catch(e){}
  }
  updateCountdowns();
 }
 refresh();
 updateCountdowns();
 setInterval(refresh,5000);
 setInterval(updateCountdowns,1000);
})();
</script>
<?php render_page_end(); ?>
