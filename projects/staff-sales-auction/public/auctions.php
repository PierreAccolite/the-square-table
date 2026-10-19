<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
function cents_to_money(int $cents):string{return number_format($cents/100,2,'.',',');}
function bid_request_uuid():string{$b=random_bytes(16);$b[6]=chr((ord($b[6])&0x0f)|0x40);$b[8]=chr((ord($b[8])&0x3f)|0x80);$h=bin2hex($b);return substr($h,0,8).'-'.substr($h,8,4).'-'.substr($h,12,4).'-'.substr($h,16,4).'-'.substr($h,20,12);}
$now=(new DateTimeImmutable('now'))->format('Y-m-d H:i:s');
$flash=(string)($_SESSION['bid_flash']??'');unset($_SESSION['bid_flash']);
$auctions=db()->query("SELECT id,title,description,starts_at,ends_at,status,anti_sniping_window_seconds FROM auctions WHERE status IN ('scheduled','open') ORDER BY starts_at,id")->fetchAll();
$itemsByAuction=[];
if($auctions){
 $ids=array_map(static fn($a)=>(int)$a['id'],$auctions);
 $marks=implode(',',array_fill(0,count($ids),'?'));
 $q=db()->prepare("SELECT i.id,i.auction_id,i.lot_number,i.title,i.description,i.condition_notes,i.image_path,i.opening_price_cents,i.minimum_increment_cents,i.buyout_enabled,i.buyout_price_cents,(SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count,(SELECT MAX(b.amount_cents) FROM bids b WHERE b.auction_item_id=i.id) AS high_amount FROM auction_items i WHERE i.status='published' AND auction_id IN ($marks) ORDER BY auction_id,CAST(lot_number AS INTEGER),lot_number,id");
 $q->execute($ids);foreach($q->fetchAll() as $item)$itemsByAuction[(int)$item['auction_id']][]=$item;
}
?>
<?php require dirname(__DIR__).'/app/layout.php'; render_page_start('Staff sale catalogue','catalogue',$user); ?>
<section class="welcome"><div><p class="eyebrow">STAFF SALES · CATALOGUE</p><h1>Browse staff sale items</h1><p class="muted">Bids are validated and recorded by the server. A valid bid in the final two minutes extends the closing time by two minutes.</p></div><div class="welcome-icon">▤</div></section>
<?php if($flash!==''): ?><div class="<?= str_starts_with($flash,'Bid accepted:')?'notice':'alert' ?>" role="status"><?= e($flash) ?></div><?php endif; ?>
<?php if(!$auctions): ?><section class="content-panel empty-state"><strong>No staff sales are published yet</strong><p>When an administrator schedules an auction, its published items will appear here.</p></section><?php else: foreach($auctions as $auction): ?>
<section class="content-panel" id="auction-<?= (int)$auction['id'] ?>"><div class="section-heading"><div><p class="eyebrow">STAFF SALE</p><h2><?= e($auction['title']) ?></h2></div><?php $isOpen=!empty($auction['starts_at'])&&!empty($auction['ends_at'])&&(string)$auction['starts_at']<=$now&&(string)$auction['ends_at']>$now&&in_array($auction['status'],['scheduled','open'],true); ?><span class="status-pill auction-status" data-auction-id="<?= (int)$auction['id'] ?>"><?= $isOpen?'Bidding open':e(ucfirst($auction['status'])) ?></span></div>
<?php if($auction['description']): ?><p class="muted"><?= nl2br(e($auction['description'])) ?></p><?php endif; ?>
<div class="date-strip"><span><strong>Opens</strong><?= e($auction['starts_at']??'Not set') ?></span><span><strong>Closes</strong><?= e($auction['ends_at']??'Not set') ?></span></div>
<?php $items=$itemsByAuction[(int)$auction['id']]??[]; if(!$items): ?><div class="empty-state"><p>No published items in this auction yet.</p></div><?php else: ?><div class="catalogue-grid"><?php foreach($items as $item):
 $high=$item['high_amount']===null?null:(int)$item['high_amount'];
 $minimum=$high===null?max(1,(int)$item['opening_price_cents']):$high+(int)$item['minimum_increment_cents'];
 $canBid=has_permission($user,'bidder')&&$isOpen;
 ?><article class="catalogue-card"><div class="catalogue-lot">LOT <?= e($item['lot_number']) ?></div>
<?php if($item['image_path']): ?><img class="catalogue-image" src="<?= e(app_url('item-image.php?id='.(int)$item['id'])) ?>" alt="<?= e($item['title']) ?>" loading="lazy"><?php else: ?><div class="catalogue-image-placeholder">No image yet</div><?php endif; ?>
<h3><?= e($item['title']) ?></h3><?php if($item['description']): ?><p><?= nl2br(e($item['description'])) ?></p><?php endif; ?><?php if($item['condition_notes']): ?><p class="condition-note"><strong>Condition:</strong> <?= nl2br(e($item['condition_notes'])) ?></p><?php endif; ?>
<div class="price-line"><span>Opening price</span><strong>N$ <?= e(cents_to_money((int)$item['opening_price_cents'])) ?></strong></div>
<div class="price-line"><span>Current high bid</span><strong class="current-high" data-item-id="<?= (int)$item['id'] ?>"><?= $high===null?'No bids yet':'N$ '.e(cents_to_money($high)) ?></strong></div>
<div class="catalogue-meta">Minimum next bid: <span class="minimum-next" data-item-id="<?= (int)$item['id'] ?>">N$ <?= e(cents_to_money($minimum)) ?></span></div><div class="catalogue-meta"><span class="bid-count" data-item-id="<?= (int)$item['id'] ?>"><?= (int)$item['bid_count'] ?></span> bid(s) recorded</div>
<?php if((int)($item['buyout_enabled']??0)===1): ?><div class="price-line"><span>Buyout price</span><strong>N$ <?= e(cents_to_money((int)$item['buyout_price_cents'])) ?></strong></div><div class="catalogue-meta">Buyout transactions are not enabled.</div><?php endif; ?>
<?php if(has_permission($user,'procurement')||has_permission($user,'custodian')): ?><p><a class="text-link" href="<?= e(app_url('request-item-change.php?id='.(int)$item['id'])) ?>">Request item change →</a></p><?php endif; ?>
<?php if($canBid): ?><form method="post" action="<?= e(app_url('place-bid.php')) ?>" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="item_id" value="<?= (int)$item['id'] ?>"><input type="hidden" name="return_auction" value="<?= (int)$auction['id'] ?>"><input type="hidden" name="client_request_id" value="<?= e(bid_request_uuid()) ?>"><label for="bid-<?= (int)$item['id'] ?>">Your bid (N$)</label><input id="bid-<?= (int)$item['id'] ?>" data-item-id="<?= (int)$item['id'] ?>" name="amount" type="number" min="<?= e(number_format($minimum/100,2,'.','')) ?>" step="0.01" required inputmode="decimal" placeholder="<?= e(number_format($minimum/100,2,'.','')) ?>"><p class="form-help">Minimum N$ <?= e(cents_to_money($minimum)) ?>. Your bid is final once accepted.</p><button class="button button-primary" type="submit">Place bid →</button></form><?php elseif(!$isOpen): ?><span class="status-pill"><?= !empty($auction['starts_at'])&&(string)$auction['starts_at']>$now?'Not open yet':'Bidding closed' ?></span><?php else: ?><span class="status-pill">Bidding permission required</span><?php endif; ?>
</article><?php endforeach; ?></div><?php endif; ?></section><?php endforeach; endif; ?>
<script>
(function(){
 const statusUrl=<?= json_encode(app_url('bid-status.php'),JSON_HEX_TAG|JSON_HEX_AMP|JSON_HEX_APOS|JSON_HEX_QUOT) ?>;
 const money=cents=>new Intl.NumberFormat('en-NA',{minimumFractionDigits:2,maximumFractionDigits:2}).format(cents/100);
 async function refreshBidStatus(){
  try{
   const response=await fetch(statusUrl,{cache:'no-store',headers:{'Accept':'application/json'}});
   if(!response.ok)return;
   const payload=await response.json();
   for(const item of payload.items||[]){
    const high=document.querySelector('.current-high[data-item-id="'+item.id+'"]');
    const minimum=document.querySelector('.minimum-next[data-item-id="'+item.id+'"]');
    const count=document.querySelector('.bid-count[data-item-id="'+item.id+'"]');
    const auctionStatus=document.querySelector('.auction-status[data-auction-id="'+item.auction_id+'"]');
    const input=document.querySelector('input[name="amount"][data-item-id="'+item.id+'"]');
    const form=input?input.closest('form'):null;
    const submit=form?form.querySelector('button[type="submit"]'):null;
    if(high)high.textContent=item.high_cents===null?'No bids yet':'N$ '+money(item.high_cents);
    if(minimum)minimum.textContent='N$ '+money(item.minimum_cents);
    if(count)count.textContent=String(item.bid_count);
    if(auctionStatus)auctionStatus.textContent=item.auction_display_status;
    if(input){input.min=(item.minimum_cents/100).toFixed(2);input.placeholder=(item.minimum_cents/100).toFixed(2);if(!item.auction_can_bid)input.disabled=true;}
    if(submit&&!item.auction_can_bid){submit.disabled=true;submit.textContent='Bidding closed';}
   }
  }catch(_error){/* Keep the last displayed values if polling is temporarily unavailable. */}
 }
 window.setInterval(refreshBidStatus,10000);
})();
</script>
<?php render_page_end(); ?>
