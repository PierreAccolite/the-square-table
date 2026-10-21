# Test plan — v0.9 beta development

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
- [ ] My account is available to every signed-in role and not to signed-out users.
- [ ] User can update display name and email only after confirming the current password.
- [ ] Invalid or duplicate email is rejected without changing the profile.
- [ ] User can change password only with the correct current password, matching confirmation, and at least 12 characters.
- [ ] Password change invalidates outstanding password-reset tokens and regenerates the session ID.
- [ ] Self-service profile/password changes generate audit events.
- [ ] User cannot edit username, employee number, role, account status, or permissions from My account.
- [ ] Employee navigation omits administrator-only links.
- [ ] Administrator navigation includes auctions, users and audit log.
- [ ] Auditor can view the audit log but cannot open administrator management pages.
- [ ] Disabled users lose access on their next authenticated request.
- [ ] User profile updates, role changes, password resets and status changes require CSRF tokens.
- [ ] The last active administrator cannot be disabled or demoted.
- [ ] Power user can submit item-change requests but cannot perform procurement/custodian review unless separately granted that permission.
- [ ] Approval navigation is visible only to users with the relevant review permissions.
- [ ] Employee can view only their own bid history and notifications.
- [ ] Bulk user import accepts legacy CSVs without optional email/permissions columns.
- [ ] Bulk user import validates optional email addresses and assigns only recognised permissions.
- [ ] Duplicate email addresses are skipped with a useful row-level explanation.

## Bidding and auction finalisation (test on a disposable database)
- [ ] Only signed-in bidders can submit bids; a scheduled auction's bid form stays disabled until opening and is disabled at closing.
- [ ] Server rejects bids before the start time and at/after the closing deadline.
- [ ] First accepted bid must be positive and meet the opening price; later bids must meet current high bid plus minimum increment.
- [ ] Malformed amounts, negative values, excessive precision and invalid request IDs are rejected.
- [ ] Replaying the same client request ID cannot create a second bid.
- [ ] Concurrent bids are serialized and cannot both pass using the same stale high bid.
- [ ] A valid bid with two minutes or less remaining extends the closing deadline by two minutes.
- [ ] A bid outside the anti-sniping window does not change the deadline.
- [ ] Accepted bids and anti-sniping changes are audited.
- [ ] Public catalogue and bidder pages never disclose another bidder's identity.
- [ ] Expired auctions reject new bids even if the CLI finaliser has not yet run.
- [ ] CLI finalisation closes expired auctions and creates exactly one result per published item.
- [ ] Highest accepted bid wins; an item without bids is marked unsold.
- [ ] Re-running finalisation does not duplicate results.
- [ ] Expiry moves a closed auction into procurement review; results remain hidden from employees.
- [ ] Winner notifications and emails are not sent before results are approved and released.
- [ ] Procurement can set unsold items to Pending, Hold or Withdrawn with an audit event.
- [ ] A different approver can release results; the procurement reviewer cannot approve their own close-out.
- [ ] Results become visible to staff and winner notifications become available only after release.
- [ ] Pending unsold items can be copied into a future draft auction while original bids and results remain unchanged.
- [ ] My Results exposes only the signed-in user's own outcomes.
- [ ] Admin results and CSV reports include final price and notification status.
- [ ] Missing/invalid winner email is recorded as no_email; mail transport failures are retryable.
- [ ] Winner email contains only that winner's item/result and the administrator-configured payment/collection instructions.
- [ ] Winner instructions are not sent to losing bidders and do not claim payment has been received.
- [ ] Mail disabled setting suppresses reset and result email delivery while in-app notifications remain available.
- [ ] A configured cron run executes the finalisation job under the web application's OS account.

## Two-stage procurement approval
- [ ] Power user can submit a change request with a reason and before/after values.
- [ ] Item is not changed during submission or procurement/custodian review.
- [ ] Requester cannot perform either review stage.
- [ ] Procurement/custodian reviewer can approve to send the request to line management or reject it.
- [ ] The same person cannot complete both procurement and line-management stages.
- [ ] Final approval applies the change only if the item still matches the saved before-state.
- [ ] Changed item is marked stale and must be resubmitted rather than overwriting newer changes.
- [ ] Both review identities, timestamps, comments and decisions are auditable.
- [ ] Duplicate lot numbers are rejected during final approval without partially applying the change.

## Auction countdown and result instructions
- [ ] Countdown displays time remaining and updates once per second.
- [ ] Countdown is calibrated using the server clock and refreshes deadline after anti-sniping extension.
- [ ] Expired countdown does not imply final results exist until the CLI finalisation job runs.
- [ ] Admin can save and test email settings without unintentionally clearing winner instructions.
- [ ] Winner instructions support line breaks and are included in result email body.

## Editing and images
- [ ] Draft and published items can be edited by an administrator.
- [ ] Awarded items are locked against editing.
- [ ] Duplicate lot numbers are rejected.
- [ ] Image upload rejects unsupported types, oversized files and excessive dimensions.
- [ ] Images are stored outside the public web root and served only through the authenticated image endpoint.
- [ ] Non-admin users cannot view draft or unpublished item images.
- [ ] Draft or scheduled auctions can be edited; closed/open/cancelled auctions are locked.

## Not implemented and therefore not testable yet
Buyout transactions, payment settlement, collection tracking inside the portal, and production security certification remain outside this milestone. The countdown updates every second and bid/deadline values refresh from the server every 10 seconds.
