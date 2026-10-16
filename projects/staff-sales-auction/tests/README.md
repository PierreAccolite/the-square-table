# Test plan — v0.6 development

Run on a disposable local SQLite database before internal deployment. Do not conduct a real auction with this release.

## Foundation
- [ ] Missing local config fails closed.
- [ ] Setup checks PHP CLI, PDO SQLite, JSON, mbstring, fileinfo, Python and write access before database setup.
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

## Self-service password reset
- [ ] User with a registered email can request a reset without revealing whether the account exists.
- [ ] Unknown, disabled, invalid-email and unregistered-email requests show the same confirmation.
- [ ] Reset token is stored as a hash, expires after 30 minutes, and cannot be reused.
- [ ] Re-requesting a reset invalidates prior unused tokens for that account.
- [ ] Passwords shorter than 12 characters and mismatched confirmations are rejected.
- [ ] Successful reset changes the password, consumes outstanding tokens, and records an audit event.
- [ ] Reset request is rate-limited to three token creations per account per hour.
- [ ] Missing mail configuration is logged without exposing account existence.
- [ ] Email addresses are unique, optional, and editable by administrators.

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

## Account and navigation
- [ ] Employee navigation omits administrator-only links.
- [ ] Administrator navigation includes auctions, users and audit log.
- [ ] Auditor can view the audit log but cannot open administrator management pages.
- [ ] Disabled users lose access on their next authenticated request.
- [ ] User profile updates, role changes, password resets and status changes require CSRF tokens.
- [ ] The last active administrator cannot be disabled or demoted.
- [ ] Employee can view only their own bid history and notifications.

## Editing and images
- [ ] Draft and published items can be edited by an administrator.
- [ ] Awarded items are locked against editing.
- [ ] Duplicate lot numbers are rejected.
- [ ] Image upload rejects unsupported types, oversized files and excessive dimensions.
- [ ] Images are stored outside the public web root and served only through the authenticated image endpoint.
- [ ] Non-admin users cannot view draft or unpublished item images.
- [ ] Draft or scheduled auctions can be edited; closed/open/cancelled auctions are locked.

## Not implemented and therefore not testable yet
Simultaneous bid handling, minimum bid validation, opening-price enforcement, deadline boundary, anti-sniping, idempotent retries, automatic closing/finalisation, winner selection, notifications and audit verification.
