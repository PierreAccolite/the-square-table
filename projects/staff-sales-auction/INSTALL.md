# Installation — Development release v0.9 beta

This folder is an installation package, not the live database. Run setup on the machine where PHP will run (for example, the internal Debian web server). This release includes bidding and result finalisation for controlled development/testing only. Do not use it for a real auction.

## Requirements
- Python 3.8+ (standard library only; no pip packages required).
- PHP 8.1+ CLI with `pdo`, `pdo_sqlite`, `json`, `mbstring` and `fileinfo`.
- Apache or another PHP-capable web server for browser access.
- A writable project `storage/` directory for the local database and runtime files.

No MariaDB/MySQL service, Composer install or external Python package is required.

## 1. Download/copy the project folder
Place the project in a suitable application directory. Do not put the project root directly under a publicly browsable web directory. The web server document root should point to the project's `public/` directory only.

## 2. Run the local setup wizard
Open a shell and change to the project root (the folder containing `README.md`), then run:

```sh
python3 tools/setup_local.py
```

The script checks Python, PHP CLI, PDO SQLite support, the migration file, required project folders and write access before making database changes. It creates `config/local.php` and the database only when missing; it never overwrites an existing local configuration or resets a populated database.

On a new installation it creates `storage/database/staff_sales_auction.sqlite` and applies the initial schema.

## 3. Create the first administrator
At the end of setup, the script starts the PHP administrator wizard. Enter:
1. An administrator username (3–80 lowercase letters, numbers, dots, underscores or hyphens).
2. Display name.
3. Optional employee number.
4. A unique password of at least 12 characters, twice for confirmation.

Password entry is hidden on Unix-like terminals where terminal control is available. The password is hashed using PHP's `password_hash()` and is not written to a setup file. No default account or password is created.

If the wizard fails after the database has been created, fix the reported issue and run `php tools/create_admin.php` from the project root to retry. It will refuse duplicate usernames. If an administrator already exists, rerunning the Python setup does not create another one.

## 4. Apply the latest additive schema update (all installations)

After the initial setup and after copying the latest files, run this from the project root to add the item-level buyout fields, permission assignments, approval queue, email addresses, password-reset tokens and auction-result storage. This step is required for both new and existing installations:

```sh
php tools/apply_migrations.php
```

The update is safe to rerun and preserves existing records. It applies migrations 002–010, including buyout settings, independent permissions (including power users), staged item-change approvals, staff email addresses, password-reset tokens, auction results, item categories, mail settings, close-out approval state and pending inventory tracking. Existing users and auction data are preserved. Buyout transactions remain disabled. Do not rerun the initial setup script just to apply this update.

## 5. Verify the installation

```sh
php tools/check_environment.php
```

All required extensions should report `OK`, and the schema should report `OK`. The database file should exist under `storage/database/`.

## 6. Configure the web server

For item-photo uploads, configure PHP with `upload_max_filesize = 6M` and `post_max_size = 8M` (or larger), then reload Apache/PHP-FPM. The application itself accepts images up to 5 MB.
Set the document root to this project's `public/` directory. Do not expose the project root, `config/`, `database/` or `storage/` through HTTP. Restrict access to the internal company network and enable HTTPS where practical.

The sample configuration enables secure session cookies by default. For isolated localhost-only HTTP testing, set `session_secure` to `false` in `config/local.php`; turn it back on when using HTTPS. Do not use plain HTTP over the company network for real employee passwords.

Make sure the PHP web-server account can read the project files and write to the SQLite database directory. The database must remain outside the web root.

### Email delivery for password resets and auction results

1. Open **Admin Settings → Email settings** and configure the sender display name and sender email address.
2. The portal hands messages to PHP's `mail()` function. If Postfix runs on the web server, ensure Postfix is configured to accept local mail and relay it as required. Relay host, port, TLS and credentials are managed by the host's Postfix configuration, not stored in the portal database.
3. Use the settings page to send a test message. A successful result means PHP handed the message to the configured mail transport; check the recipient inbox and Postfix logs to confirm delivery.
4. In **User management**, add a unique email address to each employee account. Existing accounts may have no email until you populate it.
5. Staff can use **My account** to update their display name/email and change their password after confirming the current password.
6. Test profile updates, password changes and reset requests with a controlled staff account. The reset request intentionally shows the same confirmation whether an account exists or not.

Reset links expire after 30 minutes and are single-use. If mail transport is not configured, the portal will not be able to deliver reset links.

## 7. Back up and restore
Back up the SQLite database file when no setup/schema changes are running. For an active deployment, use SQLite's online backup API or a short maintenance window rather than copying a database during active writes. Back up uploaded item images separately. Test restoration before relying on the backup.

## 8. Configure automatic auction closing

The server rejects bids at or after the stored closing time, but the database status and results are finalised by a CLI job. Configure it to run every minute as the same operating-system account that can write to the application database.

On the Debian host, first ensure the log directory exists and is writable by the web application account:

```sh
mkdir -p storage/logs
chown www-data:www-data storage/logs
```

Then edit the www-data user's crontab:

```sh
crontab -u www-data -e
```

Add this line, adjusting the project path if needed:

```cron
* * * * * cd /var/www/html/staff-sales-auction && /usr/bin/php tools/finalize_auctions.php >> storage/logs/finalize.log 2>&1
```

Run a one-time manual check first:

```sh
cd /var/www/html/staff-sales-auction
sudo -u www-data /usr/bin/php tools/finalize_auctions.php
```

The command closes expired scheduled/open auctions, stores one result per published item, creates an internal winner notification, and attempts winner email delivery. Email failures are recorded for retry. Review storage/logs/finalize.log and the administrator Results page. Ensure the configured mail_from and PHP mail() transport work before expecting email.

## Before operational use

The v0.7 bidding and closing code has passed PHP syntax checks, but syntax checks are not a runtime or concurrency test. Use a disposable database to verify bid races, minimum bids, deadline boundaries, two-minute anti-sniping, repeat finalisation, result privacy, email retries, and backup/restore. Configure HTTPS and review filesystem permissions. Do not use this development release for a real or business-critical auction.
