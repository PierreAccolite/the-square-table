# Installation — Development release v0.5

This folder is an installation package, not the live database. Run setup on the machine where PHP will run (for example, the internal Debian web server). This release is for controlled development/testing only and is not yet a live auction system.

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

After the initial setup and after copying the latest files, run this from the project root to add the item-level buyout fields, permission assignments and approval queue. This step is required for both new and existing installations:

```sh
php tools/apply_migrations.php
```

The update is safe to rerun and preserves existing records. It adds the buyout fields, independent permission assignments and the item-change approval queue. Buyout transactions are not enabled yet. Do not rerun the initial setup script just to apply this update.

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

## 7. Back up and restore
Back up the SQLite database file when no setup/schema changes are running. For an active deployment, use SQLite's online backup API or a short maintenance window rather than copying a database during active writes. Back up uploaded item images separately. Test restoration before relying on the backup.

## Before production
Implement and test bid processing, transaction/locking behaviour, anti-sniping, closing/finalisation, admin workflows, backup/restore and security tests. Configure HTTPS, review filesystem permissions and test restores. Do not run a real auction on v0.5.
