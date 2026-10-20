<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin','auditor');
$filter=trim((string)($_GET['event']??''));$page=max(1,(int)($_GET['page']??1));$perPage=50;$offset=($page-1)*$perPage;
$where='';$params=[];if($filter!==''){$where='WHERE e.event_type LIKE :event OR e.entity_type LIKE :event OR e.details_json LIKE :event';$params['event']='%'.$filter.'%';}
$countQ=db()->prepare('SELECT COUNT(*) FROM audit_events e '.$where);$countQ->execute($params);$total=(int)$countQ->fetchColumn();
$sql="SELECT e.id,e.actor_user_id,e.event_type,e.entity_type,e.entity_id,e.ip_address,e.user_agent,e.details_json,e.created_at,
             actor.username AS actor_username,actor.display_name AS actor_name,
             CASE
               WHEN e.entity_type='user' THEN COALESCE(target_user.display_name,target_user.username,'User ID '||e.entity_id)
               WHEN e.entity_type='auction' THEN COALESCE(target_auction.title,'Auction ID '||e.entity_id)
               WHEN e.entity_type='auction_item' THEN CASE WHEN target_item.id IS NOT NULL THEN 'Lot '||target_item.lot_number||' — '||target_item.title ELSE 'Item ID '||e.entity_id END
               WHEN e.entity_type='bid' THEN CASE WHEN bid_item.id IS NOT NULL THEN 'Lot '||bid_item.lot_number||' — '||bid_item.title ELSE 'Bid ID '||e.entity_id END
               ELSE COALESCE(e.entity_type,'Event')||' '||COALESCE(e.entity_id,'')
             END AS target_label
      FROM audit_events e
      LEFT JOIN users actor ON actor.id=e.actor_user_id
      LEFT JOIN users target_user ON e.entity_type='user' AND target_user.id=CAST(e.entity_id AS INTEGER)
      LEFT JOIN auctions target_auction ON e.entity_type='auction' AND target_auction.id=CAST(e.entity_id AS INTEGER)
      LEFT JOIN auction_items target_item ON e.entity_type='auction_item' AND target_item.id=CAST(e.entity_id AS INTEGER)
      LEFT JOIN auction_items bid_item ON e.entity_type='bid' AND bid_item.id=CAST(json_extract(e.details_json,'$.auction_item_id') AS INTEGER)
      ".$where." ORDER BY e.created_at DESC,e.id DESC LIMIT :limit OFFSET :offset";
$q=db()->prepare($sql);foreach($params as $key=>$value)$q->bindValue(':'.$key,$value);$q->bindValue(':limit',$perPage,PDO::PARAM_INT);$q->bindValue(':offset',$offset,PDO::PARAM_INT);$q->execute();$events=$q->fetchAll();
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Audit log','audit',$user);
?>
<section class="welcome"><div><p class="eyebrow">SECURITY & ACCOUNTABILITY</p><h1>Audit log</h1><p class="muted">Review sign-ins, bids and important changes. This page is read-only.</p></div><div class="welcome-icon">≋</div></section>
<section class="content-panel"><form method="get" class="inline-form"><label for="event">Search events, targets or details</label><input id="event" name="event" value="<?= e($filter) ?>" placeholder="e.g. bid.accepted, Lot 0042, username"><button class="button button-quiet" type="submit">Search</button><?php if($filter!==''): ?><a class="text-link" href="<?= e(app_url('admin/audit.php')) ?>">Clear</a><?php endif; ?></form><p class="form-help"><?= $total ?> matching events · showing <?= $total===0?0:$offset+1 ?>–<?= min($offset+$perPage,$total) ?></p><div class="table-wrap"><table class="data-table"><thead><tr><th>Time</th><th>Actor</th><th>Event</th><th>Target / details</th><th>IP address</th></tr></thead><tbody><?php if(!$events): ?><tr><td colspan="5">No audit events match this search.</td></tr><?php else: foreach($events as $ev): $details=json_decode((string)($ev['details_json']??''),true);if(!is_array($details))$details=[];$summary=[];if(isset($details['amount_cents']))$summary[]='Bid N$ '.number_format((int)$details['amount_cents']/100,2,'.',',');if(isset($details['role']))$summary[]='Role: '.$details['role'];if(isset($details['status']))$summary[]='Status: '.$details['status'];if(isset($details['username']))$summary[]='Username: '.$details['username'];if(isset($details['deadline_extended'])&&$details['deadline_extended'])$summary[]='Closing time extended';if(isset($details['reason']))$summary[]='Reason: '.$details['reason']; ?><tr><td><?= e($ev['created_at']) ?><small class="table-sub"><?= e($ev['user_agent']??'') ?></small></td><td><?= e($ev['actor_name']??'System / deleted user') ?><small class="table-sub"><?= e($ev['actor_username']??'') ?></small></td><td><code><?= e($ev['event_type']) ?></code></td><td><strong><?= e($ev['target_label']) ?></strong><?php if($summary): ?><small class="table-sub"><?= e(implode(' · ',$summary)) ?></small><?php endif; ?><?php if($details && !$summary): ?><details><summary>View event data</summary><pre><?= e(json_encode($details,JSON_PRETTY_PRINT|JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE)) ?></pre></details><?php endif; ?></td><td><?= e($ev['ip_address']??'—') ?></td></tr><?php endforeach; endif; ?></tbody></table></div><div class="pager"><?php if($page>1): ?><a class="button button-quiet" href="?<?= e(http_build_query(['event'=>$filter,'page'=>$page-1])) ?>">← Newer</a><?php endif; ?><?php if($offset+$perPage<$total): ?><a class="button button-quiet" href="?<?= e(http_build_query(['event'=>$filter,'page'=>$page+1])) ?>">Older →</a><?php endif; ?></div></section>
<?php render_page_end(); ?>
