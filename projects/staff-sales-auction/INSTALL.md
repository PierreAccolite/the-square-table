# Installation — Foundation v0.1

For controlled development/testing on an internal server only. This is not yet a live auction system.

## Requirements
- PHP 8.1+ with PDO, pdo_mysql, JSON and mbstring.
- Apache or another PHP-capable web server.
- MariaDB 10.6+ recommended, or compatible MySQL.
- A dedicated database and account for this application.

## 1. Create the database
Create an empty database named `staff_sales_auction` (or your chosen name) and a dedicated application account. Grant only the privileges required for this application. Use a separate migration/admin account for schema setup where practical. Do not commit credentials.

## 2. Configure
Copy `config/config.example.php` to `config/local.php` and set local values. The local file is ignored by Git. Set `base_path` to the URL path mapping to `public/`; leave empty if the virtual-host document root points directly to `public/`.

## 3. Apply the schema
Apply once to the new empty database:
```sh
mysql -u <migration-user> -p staff_sales_auction < database/migrations/001_initial_schema.sql
```

## 4. Configure the web server
Set the document root to this project's `public/` directory. Do not expose the project root, `config/`, `database/` or `storage/` to HTTP. Restrict access to the internal company network and enable HTTPS where practical. Keep uploaded files outside the public document root.

## 5. Create the first administrator
From the project root:
```sh
php tools/create_admin.php
```
Follow the prompts. Use a unique password of at least 12 characters and store it according to company policy. No default account/password is created.

## 6. Check environment
```sh
php tools/check_environment.php
```
Open the internal site and sign in.

## Before production
Implement and test bid processing, transaction/locking behaviour, anti-sniping, closing/finalisation, admin workflows, backup/restore and security tests. Configure HTTPS, review permissions and test restores. Do not run a real auction on v0.1.
