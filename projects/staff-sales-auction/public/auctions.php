<?php
declare(strict_types=1);
require dirname(__DIR__).'/app/bootstrap.php';
$user=require_login();
function cents_to_money(int $cents):string{return number_format($cents/100,2,'.',',');}
function bid_request_uuid():string{$b=random_bytes(16);$b[6]=chr((ord($b[6])&0x0f)|0x40);$b[8]=chr((ord($b[8])&0x3f)|0x80);$h=bin2hex($b);return substr($h,0,8).'-'.substr($h,8,4).'-'.substr($h,12,4).'-'.substr($h,16,4).'-'.substr($h,20,12);}
$now=(new DateTimeImmutable('now'))->format('Y-m-d H:i:s');
$categoryFilter=trim((string)($_GET['category']??''));
$bidFilter=(string)($_GET['my_bids']??'all');if(!in_array($bidFilter,['all','bid','unbid'],true))$bidFilter='all';
$stateFilter=(string)($_GET['state']??'active');if(!in_array($stateFilter,['active','open','upcoming','closed'],true))$stateFilter='active';
$categories=db()->query('SELECT name FROM item_categories WHERE is_active=1 ORDER BY name COLLATE NOCASE')->fetchAll(PDO::FETCH_COLUMN);
$flash=(string)($_SESSION['bid_flash']??'');unset($_SESSION['bid_flash']);
$auctionSql="SELECT id,title,description,starts_at,ends_at,status,anti_sniping_window_seconds FROM auctions WHERE ";
$auctionParams=[];
if($stateFilter==='closed'){$auctionSql.="(status='closed' OR (status IN ('scheduled','open') AND ends_at IS NOT NULL AND ends_at<=:now))";$auctionParams['now']=$now;}
elseif($stateFilter==='open'){$auctionSql.="status IN ('scheduled','open') AND starts_at IS NOT NULL AND starts_at<=:start_now AND ends_at IS NOT NULL AND ends_at>:end_now";$auctionParams['start_now']=$now;$auctionParams['end_now']=$now;}
elseif($stateFilter==='upcoming'){$auctionSql.="status IN ('scheduled','open') AND (starts_at IS NULL OR starts_at>:now)";$auctionParams['now']=$now;}
else{$auctionSql.="status IN ('scheduled','open')";}
$auctionSql.=" ORDER BY starts_at,id";
$auctionQuery=db()->prepare($auctionSql);$auctionQuery->execute($auctionParams);$auctions=$auctionQuery->fetchAll();
$itemsByAuction=[];
if($auctions){
 $ids=array_map(static fn($a)=>(int)$a['id'],$auctions);
 $marks=implode(',',array_map(static fn($n)=>':auction'.$n,array_keys($ids)));
 $itemStatusPredicate=$stateFilter==='closed'?"i.status IN ('published','awarded')":"i.status='published'";
 $itemSql="SELECT i.id,i.auction_id,i.lot_number,i.title,i.description,i.condition_notes,i.category,i.image_path,i.opening_price_cents,i.minimum_increment_cents,i.buyout_enabled,i.buyout_price_cents,(SELECT COUNT(*) FROM bids b WHERE b.auction_item_id=i.id) AS bid_count,(SELECT MAX(b.amount_cents) FROM bids b WHERE b.auction_item_id=i.id) AS high_amount,(SELECT COUNT(*) FROM bids mine WHERE mine.auction_item_id=i.id AND mine.bidder_user_id=:uid_count) AS my_bid_count FROM auction_items i WHERE ".$itemStatusPredicate." AND i.auction_id IN ($marks)";
 $itemParams=['uid_count'=>(int)$user['id']];
 foreach($ids as $idx=>$auctionId)$itemParams['auction'.$idx]=$auctionId;
 if($categoryFilter!==''){$itemSql.=" AND i.category=:category";$itemParams['category']=$categoryFilter;}
 if($bidFilter==='bid'){$itemSql.=" AND EXISTS(SELECT 1 FROM bids mine WHERE mine.auction_item_id=i.id AND mine.bidder_user_id=:uid_filter)";$itemParams['uid_filter']=(int)$user['id'];}
 elseif($bidFilter==='unbid'){$itemSql.=" AND NOT EXISTS(SELECT 1 FROM bids mine WHERE mine.auction_item_id=i.id AND mine.bidder_user_id=:uid_filter)";$itemParams['uid_filter']=(int)$user['id'];}
 $itemSql.=" ORDER BY auction_id,CAST(lot_number AS INTEGER),lot_number,id";
 $q=db()->prepare($itemSql);$q->execute($itemParams);foreach($q->fetchAll() as $item)$itemsByAuction[(int)$item['auction_id']][]=$item;
}
?>
<?php require dirname(__DIR__).'/app/layout.php'; render_page_start('Staff sale catalogue','catalogue',$user); ?>
<section class="welcome"><div><p class="eyebrow">STAFF SALES · CATALOGUE</p><h1>Browse staff sale items</h1><p class="muted">Search and filter published lots. A valid bid in the final two minutes extends the closing time by two minutes.</p></div><div class="welcome-icon">▤</div></section>
<section class="content-panel"><form method="get" class="inline-form catalogue-filters">
 <div><label for="state">Auction status</label><select id="state" name="state"><option value="active" <?= $stateFilter==='active'?'selected':'' ?>>Active / upcoming auctions</option><option value="open" <?= $stateFilter==='open'?'selected':'' ?>>Open for bidding now</option><option value="upcoming" <?= $stateFilter==='upcoming'?'selected':'' ?>>Upcoming</option><option value="closed" <?= $stateFilter==='closed'?'selected':'' ?>>Closed lots</option></select></div>
 <div><label for="category">Category</label><select id="category" name="category"><option value="">All categories</option><?php foreach($categories as $cat): ?><option value="<?= e($cat) ?>" <?= $categoryFilter===(string)$cat?'selected':'' ?>><?= e($cat) ?></option><?php endforeach; ?></select></div>
 <div><label for="my_bids">My bidding</label><select id="my_bids" name="my_bids"><option value="all" <?= $bidFilter==='all'?'selected':'' ?>>All lots</option><option value="bid" <?= $bidFilter==='bid'?'selected':'' ?>>Lots I've bid on</option><option value="unbid" <?= $bidFilter==='unbid'?'selected':'' ?>>Lots I haven't bid on</option></select></div>
 <button class="button button-primary" type="submit">Apply filters</button><?php if($categoryFilter!==''||$bidFilter!=='all'||$stateFilter!=='active'): ?><a class="text-link" href="<?= e(app_url('auctions.php')) ?>">Clear filters</a><?php endif; ?>
</form></section>
<?php if($flash!==''): ?><div class="<?= str_starts_with($flash,'Bid accepted:')?'notice':'alert' ?>" role="status"><?= e($flash) ?></div><?php endif; ?>
<?php if(!$auctions): ?><section class="content-panel empty-state"><strong>No staff sales are published yet</strong><p>When an administrator schedules an auction, its published items will appear here.</p></section><?php else: foreach($auctions as $auction): ?>
<section class="content-panel" id="auction-<?= (int)$auction['id'] ?>"><div class="section-heading"><div><p class="eyebrow">STAFF SALE</p><h2><?= e($auction['title']) ?></h2></div><?php $isOpen=!empty($auction['starts_at'])&&!empty($auction['ends_at'])&&(string)$auction['starts_at']<=$now&&(string)$auction['ends_at']>$now&&in_array($auction['status'],['scheduled','open'],true); ?><span class="status-pill auction-status" data-auction-id="<?= (int)$auction['id'] ?>"><?= $isOpen?'Bidding open':e(ucfirst($auction['status'])) ?></span></div>
<?php if($auction['description']): ?><p class="muted"><?= nl2br(e($auction['description'])) ?></p><?php endif; ?>
<div class="date-strip"><span><strong>Opens</strong><?= e($auction['starts_at']??'Not set') ?></span><span><strong>Closes</strong><?= e($auction['ends_at']??'Not set') ?></span><span><strong>Time remaining</strong><span class="auction-countdown" data-auction-id="<?= (int)$auction['id'] ?>" data-ends-at-epoch="<?= !empty($auction['ends_at'])?(new DateTimeImmutable((string)$auction['ends_at']))->getTimestamp():'' ?>">Calculating…</span></span></div>
<?php $items=$itemsByAuction[(int)$auction['id']]??[]; if(!$items): ?><div class="empty-state"><p>No published items in this auction yet.</p></div><?php else: ?><div class="catalogue-grid"><?php foreach($items as $item):
 $high=$item['high_amount']===null?null:(int)$item['high_amount'];
 $minimum=$high===null?max(1,(int)$item['opening_price_cents']):$high+(int)$item['minimum_increment_cents'];
 $canBid=has_permission($user,'bidder')&&in_array($auction['status'],['scheduled','open'],true);
 ?><article class="catalogue-card"><div class="catalogue-lot">LOT <?= e($item['lot_number']) ?></div><span class="status-pill catalogue-category"><?= e($item['category']??'Uncategorised') ?></span>
<?php if($item['image_path']): ?><img class="catalogue-image" src="<?= e(app_url('item-image.php?id='.(int)$item['id'])) ?>" alt="<?= e($item['title']) ?>" loading="lazy"><?php else: ?><div class="catalogue-image-placeholder">No image yet</div><?php endif; ?>
<h3><?= e($item['title']) ?></h3><?php if($item['description']): ?><p><?= nl2br(e($item['description'])) ?></p><?php endif; ?><?php if($item['condition_notes']): ?><p class="condition-note"><strong>Condition:</strong> <?= nl2br(e($item['condition_notes'])) ?></p><?php endif; ?>
<div class="price-line"><span>Opening price</span><strong>N$ <?= e(cents_to_money((int)$item['opening_price_cents'])) ?></strong></div>
<div class="price-line"><span>Current high bid</span><strong class="current-high" data-item-id="<?= (int)$item['id'] ?>"><?= $high===null?'No bids yet':'N$ '.e(cents_to_money($high)) ?></strong></div>
<div class="catalogue-meta">Minimum next bid: <span class="minimum-next" data-item-id="<?= (int)$item['id'] ?>">N$ <?= e(cents_to_money($minimum)) ?></span></div><div class="catalogue-meta"><span class="bid-count" data-item-id="<?= (int)$item['id'] ?>"><?= (int)$item['bid_count'] ?></span> bid(s) recorded</div>
<?php if((int)$item['my_bid_count']>0): ?><div class="catalogue-meta">You've placed <?= (int)$item['my_bid_count'] ?> bid(s) on this lot</div><?php endif; ?>
<?php if((int)($item['buyout_enabled']??0)===1): ?><div class="price-line"><span>Buyout price</span><strong>N$ <?= e(cents_to_money((int)$item['buyout_price_cents'])) ?></strong></div><div class="catalogue-meta">Buyout transactions are not enabled.</div><?php endif; ?>
<?php if(has_permission($user,'procurement')||has_permission($user,'custodian')): ?><p><a class="text-link" href="<?= e(app_url('request-item-change.php?id='.(int)$item['id'])) ?>">Request item change →</a></p><?php endif; ?>
<?php if($canBid): ?><form method="post" action="<?= e(app_url('place-bid.php')) ?>" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><input type="hidden" name="item_id" value="<?= (int)$item['id'] ?>"><input type="hidden" name="return_auction" value="<?= (int)$auction['id'] ?>"><input type="hidden" name="client_request_id" value="<?= e(bid_request_uuid()) ?>"><label for="bid-<?= (int)$item['id'] ?>">Your bid (N$)</label><input id="bid-<?= (int)$item['id'] ?>" data-item-id="<?= (int)$item['id'] ?>" name="amount" type="number" min="<?= e(number_format($minimum/100,2,'.','')) ?>" step="0.01" required inputmode="decimal" placeholder="<?= e(number_format($minimum/100,2,'.','')) ?>" <?= $isOpen?'':'disabled' ?>><p class="form-help">Minimum N$ <?= e(cents_to_money($minimum)) ?>. Your bid is final once accepted.</p><button class="button button-primary" type="submit" <?= $isOpen?'':'disabled' ?>><?= $isOpen?'Place bid →':'Bidding not open yet' ?></button></form><?php elseif(!$isOpen): ?><span class="status-pill"><?= !empty($auction['starts_at'])&&(string)$auction['starts_at']>$now?'Not open yet':'Bidding closed' ?></span><?php else: ?><span class="status-pill">Bidding permission required</span><?php endif; ?>
</article><?php endforeach; ?></div><?php endif; ?></section><?php endforeach; endif; ?>
<script>
(function(){
 const statusUrl=<?= json_encode(app_url('bid-status.php'),JSON_HEX_TAG|JSON_HEX_AMP|JSON_HEX_APOS|JSON_HEX_QUOT) ?>;
 const money=cents=>new Intl.NumberFormat('en-NA',{minimumFractionDigits:2,maximumFractionDigits:2}).format(cents/100);
 let serverClockOffsetMs=0;
 function renderCountdowns(){
  const now=Math.floor((Date.now()-serverClockOffsetMs)/1000);
  for(const timer of document.querySelectorAll('.auction-countdown')){
   const end=Number(timer.dataset.endsAtEpoch||0);
   if(!end){timer.textContent='Not scheduled';continue;}
   let remaining=end-now;
   if(remaining<=0){timer.textContent='Closing / closed';timer.classList.add('is-expired');continue;}
   timer.classList.remove('is-expired');
   const days=Math.floor(remaining/86400);remaining%=86400;
   const hours=Math.floor(remaining/3600);remaining%=3600;
   const minutes=Math.floor(remaining/60);const seconds=remaining%60;
   const clock=[hours,minutes,seconds].map(n=>String(n).padStart(2,'0')).join(':');
   timer.textContent=days>0?days+'d '+clock:clock;
  }
 }
 async function refreshBidStatus(){
  try{
   const response=await fetch(statusUrl,{cache:'no-store',headers:{'Accept':'application/json'}});
   if(!response.ok)return;
   const payload=await response.json();
   if(typeof payload.server_time_epoch==='number')serverClockOffsetMs=Date.now()-(payload.server_time_epoch*1000);
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
    if(auctionStatus){auctionStatus.textContent=item.auction_display_status;if(item.ends_at_epoch!==null&&item.ends_at_epoch!==undefined){const timer=document.querySelector('.auction-countdown[data-auction-id="'+item.auction_id+'"]');if(timer)timer.dataset.endsAtEpoch=String(item.ends_at_epoch);}}
    if(input){input.min=(item.minimum_cents/100).toFixed(2);input.placeholder=(item.minimum_cents/100).toFixed(2);input.disabled=!item.auction_can_bid;}
    if(submit){submit.disabled=!item.auction_can_bid;submit.textContent=item.auction_can_bid?'Place bid →':(item.auction_display_status==='Scheduled'?'Bidding not open yet':'Bidding closed');}
   }
  }catch(_error){/* Keep the last displayed values if polling is temporarily unavailable. */}
 }
 renderCountdowns();
 window.setInterval(renderCountdowns,1000);
 window.setInterval(refreshBidStatus,10000);
})();
</script>
<?php render_page_end(); ?>
