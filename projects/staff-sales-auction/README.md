# Staff Sales Auction Portal

**Version:** 0.1.0 — foundation release  
**Status:** Foundation scaffold; not yet ready for live auctions.

A new, independent internal staff auction application for the Square Table projects collection. Existing Staff Sales and Request Management Portal projects are not modified.

## Agreed design
- Internal company network only; individual portal-managed employee accounts.
- PHP 8.1+, MariaDB/MySQL through PDO, responsive HTML/CSS/JavaScript.
- Server-side bid validation and transactional processing in a later milestone.
- Full accepted-bid history and audit events.
- Anti-sniping: a valid accepted bid with **two minutes or less remaining** resets the deadline to two minutes from acceptance.
- Ordinary employees see prices and auction status, not other bidders' identities.

## Included in v0.1
- Initial MariaDB schema and migration.
- Local configuration example; secrets are not committed.
- Login/logout foundation, password hashing, CSRF tokens, secure session settings and basic login throttling.
- Employee landing dashboard and restricted administrator dashboard.
- CLI-only first administrator creation tool.
- Environment/database diagnostic tool and installation notes.

## Not included yet
Live auction pages, item-management forms, bid submission, automatic countdown/polling, anti-sniping execution, finalisation/award workflow, notifications or production security certification. The schema includes initial auction-domain tables for later milestones, but no real bids can be placed in this release.

## Layout
- `public/` — web-accessible entry points and assets only.
- `app/` — bootstrap, database, session, CSRF and audit helpers.
- `config/` — example configuration; local secrets stay untracked.
- `database/migrations/` — versioned SQL schema.
- `storage/` — runtime logs and item images; not public.
- `tools/` — CLI setup and diagnostics.
- `tests/` — test plan and future automated tests.

The database and audit trail improve accountability but cannot, by themselves, make tampering impossible. Production requires restricted DB permissions, protected backups, HTTPS where practical, and audit review. Do not use real auction data until the auction-processing milestone is implemented and tested.
