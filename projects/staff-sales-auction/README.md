# Staff Sales Auction Portal

**Version:** 0.7.0 — transactional bidding, auction closing and final results  
**Status:** Internal development/testing only; live bidding is implemented but must be validated before any real auction.

A new, independent internal staff auction application in the Square Table projects collection. Existing Staff Sales and Request Management Portal projects are not modified.

## Agreed design
- Internal company network only; individual portal-managed employee accounts.
- PHP 8.1+ with PDO SQLite; no separate database service required.
- A local SQLite database stored under `storage/database/`, outside the public web root.
- Server-side bid validation and transactional processing before live bidding is enabled.
- Full accepted-bid history and audit events.
- Anti-sniping: a valid accepted bid with **two minutes or less remaining** resets the deadline to two minutes from acceptance.
- Ordinary employees see prices and auction status, not other bidders' identities.
- The project folder is an installation package. The live database and local configuration are created on the target machine and are not committed to GitHub.

## Quick local setup
From the project root on the machine where PHP will run:

```sh
python3 tools/setup_local.py
```

The setup script checks Python, PHP CLI, required PHP extensions, directories and SQLite support. It creates the local configuration and database only if missing, applies the schema to a new empty database, then starts a command-line wizard for the first administrator. Passwords are entered interactively and are not written to a file.

To re-check the installation later:

```sh
php tools/check_environment.php
```

See [INSTALL.md](INSTALL.md) for the step-by-step walkthrough.

## Included in v0.6 development
- SQLite schema and local setup wizard.
- Login/logout foundation, password hashing, CSRF tokens, secure session settings and basic login throttling.
- Employee dashboard with a staff catalogue entry point.
- Administrator dashboard with user, auction, item, bid and audit counts.
- Create auction drafts with planned opening and closing times.
- Add catalogue items with lot number, description, condition notes, opening price and minimum bid increment.
- Store prices as integer cents.
- Publish individual catalogue items and schedule a draft auction after required dates and at least one published item are present.
- Shared sticky navigation that adapts to employee, administrator and auditor roles.
- Admin user management: create accounts, edit staff details and roles, enable/disable accounts, and reset passwords.
- CSV bulk user import with row-level validation, duplicate detection and audit events.
- Read-only admin/auditor reports for auction summaries, item catalogues, bid activity, final auction results and user accounts, with CSV exports.
- Item-level buyout toggle and configured buyout price; purchase transactions remain disabled.
- Recorded bid counts on staff catalogue cards and the admin item list.
- Independent bidder, procurement, custodian and approver permissions, combinable per account.
- Self-service password reset with single-use hashed tokens, 30-minute expiry, generic responses and rate limiting; requires staff email addresses and mail transport configuration.
- Procurement/custodian item-change requests with reason, before/after comparison, separate-person approval, stale-request protection and audit logging.
- Auction editing for draft and scheduled auctions.
- Item editing, draft/published/withdrawn status, and protected JPEG/PNG/WebP item-image uploads.
- Staff catalogue displays item photos through an authenticated image endpoint.
- Personal bid-history page, notification inbox, and read-only audit log.
- Signed-in employees can browse published items in scheduled/open auctions, see live price/countdown updates, and submit bids.
- Transactional server-side bid validation, per-item bid history and two-minute anti-sniping extensions.
- Automatic opening of scheduled auctions and idempotent close/finalisation with immutable sold/unsold result records.
- Staff-facing final results page that hides other bidders' identities; winner identities and detailed bid activity remain admin/auditor-only.
- CLI scheduler at `tools/close_auctions.php`; configure cron so auctions finalise even when nobody is browsing.
- Audit events for auction and item creation and auction scheduling.

## Not included yet
- Buyout/purchase-now transactions, despite the configurable catalogue option.
- Winner email notifications and payment/collection settlement workflows.
- Item deletion is intentionally not offered; items can be withdrawn.
- Production security certification and a completed multi-user concurrency/restore test cycle.

**Important:** this remains a development/testing release. Do not use it to conduct a real auction until the bid, closing, result, backup/restore and security test plan has passed.

## Layout
- `public/` — web-accessible entry points and assets only.
- `app/` — bootstrap, database, session, CSRF and audit helpers.
- `config/` — example configuration; local secrets stay untracked.
- `database/migrations/` — versioned SQLite schema.
- `storage/database/` — local database; not committed.
- `storage/logs/` and `storage/item-images/` — runtime data outside the public web root.
- `tools/` — local setup and diagnostics.
- `tests/` — test plan and future automated tests.

The database and audit trail improve accountability but cannot, by themselves, make tampering impossible. Protect the live database and backups with operating-system permissions. Keep the database outside the web root and test backup/restore. Configure the web server's document root to the project's `public/` directory only.
