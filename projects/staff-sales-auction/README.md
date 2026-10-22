# Staff Sales Auction Portal

**Version:** 0.11.0-beta — assigned line managers, automatic lot numbering and pending inventory  
**Status:** Internal development/testing only; bidding and result finalisation are implemented for controlled testing, not real auctions.

A new, independent internal staff auction application in the Square Table projects collection. Existing Staff Sales and Request Management Portal projects are not modified.

## Agreed design
- Internal company network only; individual portal-managed employee accounts.
- PHP 8.1+ with PDO SQLite; no separate database service required.
- A local SQLite database stored under `storage/database/`, outside the public web root.
- Server-side bid validation and transactional processing.
- Full accepted-bid history and audit events.
- SQLite write-locked bid acceptance, server-side price/deadline validation, and idempotent request identifiers.
- One-time item-level results, private bidder results, administrator results and CSV result reporting.
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

## Included in v0.8 development
- SQLite schema and local setup wizard.
- Login/logout foundation, password hashing, CSRF tokens, secure session settings and basic login throttling.
- Employee dashboard with a staff catalogue entry point.
- Administrator dashboard with user, auction, item, bid and audit counts.
- Create auction drafts with planned opening and closing times.
- Add catalogue items with lot number, description, condition notes, opening price and minimum bid increment.
- Store prices as integer cents.
- Publish individual catalogue items and schedule a draft auction after required dates and at least one published item are present.
- Shared sticky navigation that adapts to employee, administrator and auditor roles.
- Admin user management: create accounts, edit staff details and roles in a responsive edit panel, assign each employee a line manager, enable/disable accounts, and reset passwords.
- CSV bulk user import with row-level validation, duplicate detection, optional email and permission assignment, and audit events.
- Read-only admin/auditor reports for auction summaries, item catalogues, bid activity and user accounts, with CSV exports.
- Item-level buyout toggle and configured buyout price; purchase transactions remain disabled.
- Recorded bid counts on staff catalogue cards and the admin item list.
- Independent bidder, power-user, procurement, custodian and approver permissions, combinable per account. Power users submit changes; procurement/custodians review; line managers approve finally.
- Self-service account page to update display name/email and change password after confirming the current password.
- Self-service password reset with single-use hashed tokens, 30-minute expiry, generic responses and rate limiting; requires staff email addresses and mail transport configuration.
- Server-validated bidding with opening-price/minimum-increment enforcement and two-minute anti-sniping extensions.
- CLI closing/finalisation job with unique provisional result rows; winner notifications and emails are held until procurement review and separate approval release results.
- Auction close-out workflow: expiry closes bidding, procurement reviews sold/unsold outcomes, a different approver releases results, and released results become visible to staff.
- Reusable Pending Items workflow for unsold lots: procurement can mark items pending, hold or withdraw them; pending items can be copied into a future draft auction with source-item history retained.
- Private My Results page shows each employee only their own outcomes after release; a staff-facing approved Auction Results page shows sold/unsold outcomes and final prices without exposing winner identities; administrators retain the restricted detailed results dashboard.
- Auction results and final-price CSV report.
- Pending items can be selected directly from Manage Items or the dedicated Pending Items page; each copy gets the next automatic lot number in its destination auction while original lot, bids and result are retained.
- Power users can submit catalogue-change requests; procurement/custodians review first and the requester's assigned active line manager/approver gives final approval. User management supports assigning line managers, and the approval handler enforces that assignment.
- Approval workflow stores before/after values, review notes, reviewer identities/timestamps, assigned-manager enforcement, stale-request protection and audit events.
- Auction editing for draft and scheduled auctions.
- Item editing, draft/published/withdrawn status, and protected JPEG/PNG/WebP item-image uploads.
- Staff catalogue displays item photos through an authenticated image endpoint.
- Personal bid-history page grouped to one row per lot, expandable personal bid history, notification inbox, and read-only audit log.
- Bid audit events record the source IP address and user agent; audit targets resolve to lot numbers, item names or affected user names.
- Catalogue filters for active/upcoming/open/closed auctions, item category, lots already bid on and lots not yet bid on.
- Database-backed item categories maintained by authorised procurement/custodian users.
- Role-aware grouped navigation and administrator mail settings with a delivery test through the host's configured Postfix/PHP mail transport.
- Configurable winner-only email instructions for payment/collection next steps; this portal does not process payments.
- Signed-in employees can browse published items in scheduled auctions.
- Audit events for auction and item creation and auction scheduling.

## Not included yet
- Catalogue current bids, minimums and auction deadlines refresh every 10 seconds, with a live countdown calibrated to server time.
- Automatic closing depends on a scheduled CLI job; configure and verify it before testing time-based closing. Results remain hidden until procurement close-out and separate approval.
- Item deletion is intentionally not offered; items can be withdrawn.
- Buyout/purchase-now transactions remain disabled.
- Payment settlement and collection tracking inside the portal; winner emails provide configurable next-step instructions only.
- Production security certification.

**Important:** this remains a development/testing release. Bidding is enabled in code but has not been certified for real-money or business-critical auctions. Configure the finalisation job, verify mail transport, test backups/restore and exercise the test checklist on a disposable database before any operational use. Scheduling makes published catalogue items visible; the server opens bidding according to the configured start time.

## Layout
- `public/` — web-accessible entry points and assets only.
- `app/` — bootstrap, database, session, CSRF and audit helpers.
- `config/` — example configuration; local secrets stay untracked.
- `database/migrations/` — versioned SQLite schema.
- `storage/database/` — local database; not committed.
- `storage/logs/` and `storage/item-images/` — runtime data outside the public web root.
- `tools/` — local setup and diagnostics.
- `tests/` — test plan and future automated tests.

For upgrades, run `php tools/apply_migrations.php` from the installed project directory after backing up the database. Migration 011 adds the line-manager link without deleting existing user or auction data. Existing users start with no assigned manager and should be linked through User management before new item-change requests reach final approval.\n\nThe database and audit trail improve accountability but cannot, by themselves, make tampering impossible. Protect the live database and backups with operating-system permissions. Keep the database outside the web root and test backup/restore. Configure the web server's document root to the project's `public/` directory only.
