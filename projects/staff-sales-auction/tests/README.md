# Test plan — v0.2 development

Run on a disposable local SQLite database before internal deployment. Do not conduct a real auction with this release.

## Foundation
- [ ] Missing local config fails closed.
- [ ] Setup checks PHP CLI, PDO SQLite, JSON, mbstring, Python and write access before database setup.
- [ ] Setup creates the database and schema only when the database is new/empty.
- [ ] Setup does not overwrite an existing local config or database.
- [ ] Active account signs in and session ID is regenerated.
- [ ] Wrong password, unknown account and disabled account show the same generic error.
- [ ] Five failed attempts for the same username/IP within 15 minutes trigger throttling.
- [ ] Missing/invalid CSRF token rejects login/logout POSTs.
- [ ] Employee role cannot access administrator pages.
- [ ] Logout invalidates the session.
- [ ] SQLite foreign-key enforcement is enabled for each connection.
- [ ] Local configuration and runtime database files are not tracked.

## Auction catalogue
- [ ] Admin can create an auction draft with title, description and planned dates.
- [ ] Invalid date formats and closing times before/equal to opening time are rejected.
- [ ] Admin can add an item with a unique lot number per auction.
- [ ] Invalid prices, zero/negative increments and oversized fields are rejected.
- [ ] Prices display correctly in NAD and are stored as integer cents.
- [ ] Draft items and draft auctions are not visible to ordinary employees.
- [ ] Scheduling requires valid opening and closing times and at least one published item.
- [ ] Scheduling is protected by a CSRF token and only admins can perform it.
- [ ] Scheduling makes only published items visible to signed-in employees.
- [ ] Employee catalogue does not show bidder identities or private bid information.
- [ ] Auction and item creation/scheduling generate audit events.
- [ ] Public web server document root points to `public/` only.

## Not implemented and therefore not testable yet
Simultaneous bid handling, minimum bid validation, opening-price enforcement, deadline boundary, anti-sniping, idempotent retries, automatic closing/finalisation, winner selection, notifications and audit verification.
