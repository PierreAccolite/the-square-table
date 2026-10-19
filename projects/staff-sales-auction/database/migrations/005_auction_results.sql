-- Migration 005: durable, one-time auction results and winner-notification tracking.
CREATE TABLE IF NOT EXISTS auction_results (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    auction_item_id INTEGER NOT NULL UNIQUE,
    auction_id INTEGER NOT NULL,
    result_status TEXT NOT NULL CHECK(result_status IN ('sold','unsold')),
    winning_bid_id INTEGER,
    winner_user_id INTEGER,
    final_price_cents INTEGER NOT NULL DEFAULT 0 CHECK(final_price_cents >= 0),
    finalized_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),
    email_status TEXT NOT NULL DEFAULT 'not_applicable' CHECK(email_status IN ('not_applicable','pending','sent','failed','no_email')),
    email_sent_at TEXT,
    email_last_error TEXT,
    FOREIGN KEY(auction_item_id) REFERENCES auction_items(id) ON DELETE RESTRICT,
    FOREIGN KEY(auction_id) REFERENCES auctions(id) ON DELETE RESTRICT,
    FOREIGN KEY(winning_bid_id) REFERENCES bids(id) ON DELETE RESTRICT,
    FOREIGN KEY(winner_user_id) REFERENCES users(id) ON DELETE RESTRICT
);
CREATE INDEX IF NOT EXISTS idx_auction_results_auction ON auction_results(auction_id,result_status,finalized_at);
CREATE INDEX IF NOT EXISTS idx_auction_results_winner ON auction_results(winner_user_id,finalized_at);
CREATE INDEX IF NOT EXISTS idx_auction_results_email ON auction_results(email_status,finalized_at);
