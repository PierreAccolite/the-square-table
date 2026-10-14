# Test plan — v0.1

Run on a disposable local SQLite database before internal deployment.

- [ ] Missing local config fails closed.
- [ ] Setup checks PHP CLI, PDO SQLite, JSON, mbstring, Python and write access before database setup.
- [ ] Setup creates the database and schema only when the database is new/empty.
- [ ] Setup does not overwrite an existing local config or database.
- [ ] Active account signs in and session ID is regenerated.
- [ ] Wrong password, unknown account and disabled account show the same generic error.
- [ ] Five failed attempts for the same username/IP within 15 minutes trigger throttling.
- [ ] Missing/invalid CSRF token rejects login/logout POSTs.
- [ ] Employee role cannot access administrator dashboard.
- [ ] Administrator sees read-only foundation counts.
- [ ] Logout invalidates the session.
- [ ] SQLite foreign-key enforcement is enabled for each connection.
- [ ] Admin bootstrap rejects short/mismatched passwords and duplicate usernames.
- [ ] Password input is hidden where supported and never written to a setup file.
- [ ] Local configuration and runtime database files are not tracked.
- [ ] SQLite schema applies to a new empty database and all expected tables are present.

Not implemented yet: simultaneous bid handling, minimum increments, opening prices, deadline boundary, anti-sniping, idempotent retries, closing/finalisation and audit verification.
