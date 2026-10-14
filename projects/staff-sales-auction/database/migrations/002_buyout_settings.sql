-- Add item-level buyout configuration. This does not enable buyout transactions.
ALTER TABLE auction_items ADD COLUMN buyout_enabled INTEGER NOT NULL DEFAULT 0 CHECK(buyout_enabled IN (0,1));
ALTER TABLE auction_items ADD COLUMN buyout_price_cents INTEGER NULL CHECK(buyout_price_cents IS NULL OR buyout_price_cents >= 0);
