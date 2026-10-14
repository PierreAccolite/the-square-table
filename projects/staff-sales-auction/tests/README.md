# Test plan — v0.1

Run on a disposable development database before internal deployment.

- [ ] Missing local config fails closed.
- [ ] Active account signs in and session ID is regenerated.
- [ ] Wrong password, unknown account and disabled account show the same generic error.
- [ ] Five failed attempts for the same username/IP within 15 minutes trigger throttling.
- [ ] Missing/invalid CSRF token rejects login/logout POSTs.
- [ ] Employee role cannot access administrator dashboard.
- [ ] Administrator sees read-only foundation counts.
- [ ] Logout invalidates the session.
- [ ] SQL migration applies to an empty MariaDB database.
- [ ] Admin bootstrap rejects short/mismatched passwords and duplicate usernames.
- [ ] Local configuration and runtime files are not tracked.

Not implemented yet: simultaneous bid handling, minimum increments, opening prices, deadline boundary, anti-sniping, idempotent retries, closing/finalisation and audit verification.
