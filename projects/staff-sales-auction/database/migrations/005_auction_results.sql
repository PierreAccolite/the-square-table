-- Migration 005: persist immutable per-item outcomes when an auction closes.
BEGIN IMMEDIATE;
CREATE TABLE IF NOT EXISTS auction_item_results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    auction_id INTEGER NOT NULL,
    auction_item_id INTEGER NOT NULL UNIQUE,
    result_status TEXT NOT NULL CHECK(result_status IN ('sold','unsold')),
    winning_bid_id INTEGER UNIQUE,
    winner_user_id INTEGER,
    final_price_cents INTEGER CHECK(final_price_cents IS NULL OR final_price_cents >= 0),
    finalised_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),
    FOREIGN KEY(auction_id) REFERENCES auctions(id) ON DELETE RESTRICT,
    FOREIGN KEY(auction_item_id) REFERENCES auction_items(id) ON DELETE RESTRICT,
    FOREIGN KEY(winning_bid_id) REFERENCES bids(id) ON DELETE RESTRICT,
    FOREIGN KEY(winner_user_id) REFERENCES users(id) ON DELETE RESTRICT,
    CHECK(
      (result_status='sold' AND winning_bid_id IS NOT NULL AND winner_user_id IS NOT NULL AND final_price_cents IS NOT NULL)
      OR
      (result_status='unsold' AND winning_bid_id IS NULL AND winner_user_id IS NULL AND final_price_cents IS NULL)
    )
);
CREATE INDEX IF NOT EXISTS idx_auction_results_auction ON auction_item_results(auction_id,result_status);
INSERT OR IGNORE INTO schema_migrations(version) VALUES('005_auction_results');
COMMIT;
