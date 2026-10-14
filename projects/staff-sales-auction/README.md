# Staff Sales Auction Portal

**Version:** 0.1.0 — foundation release  
**Status:** Foundation scaffold; not yet ready for live auctions.

A new, independent internal staff auction application in the Square Table projects collection. Existing Staff Sales and Request Management Portal projects are not modified.

## Agreed design
- Internal company network only; individual portal-managed employee accounts.
- PHP 8.1+ with PDO SQLite; no separate database service required.
- A local SQLite database stored under `storage/database/`, outside the public web root.
- Server-side bid validation and transactional processing in a later milestone.
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

## Included in v0.1
- SQLite schema and migration.
- Local configuration example; live config and database stay untracked.
- Login/logout foundation, password hashing, CSRF tokens, secure session settings and basic login throttling.
- Employee landing dashboard and restricted administrator dashboard.
- CLI-only first administrator creation tool.
- Python setup/check wizard and PHP environment diagnostic tool.

## Not included yet
Live auction pages, item-management forms, bid submission, automatic countdown/polling, anti-sniping execution, finalisation/award workflow, notifications or production security certification. The schema includes initial auction-domain tables for later milestones, but no real bids can be placed in this release.

## Layout
- `public/` — web-accessible entry points and assets only.
- `app/` — bootstrap, database, session, CSRF and audit helpers.
- `config/` — example configuration; local secrets stay untracked.
- `database/migrations/` — versioned SQLite schema.
- `storage/database/` — local SQLite database (not public, not committed).
- `storage/logs/` and `storage/item-images/` — runtime data outside the public web root.
- `tools/` — local setup and diagnostics.
- `tests/` — test plan and future automated tests.

The database and audit trail improve accountability but cannot, by themselves, make tampering impossible. Protect the live database and backups with operating-system permissions. Keep the database outside the web root and test backup/restore. Do not use real auction data until the auction-processing milestone is implemented and tested.
