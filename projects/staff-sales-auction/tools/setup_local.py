#!/usr/bin/env python3
"""Local installer for the Staff Sales Auction Portal. Python standard library only."""
from __future__ import annotations
import os
import shutil
import sqlite3
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG_EXAMPLE = ROOT / "config" / "config.example.php"
CONFIG_LOCAL = ROOT / "config" / "local.php"
SCHEMA = ROOT / "database" / "migrations" / "001_initial_schema.sql"
DB_DEFAULT = ROOT / "storage" / "database" / "staff_sales_auction.sqlite"
EXPECTED_TABLES = {"users", "auctions", "auction_items", "bids", "audit_events", "login_attempts", "notifications"}
REQUIRED_EXTENSIONS = ("pdo", "pdo_sqlite", "json", "mbstring", "fileinfo")

def stop(message: str, code: int = 1) -> None:
    print(f"\nERROR: {message}", file=sys.stderr)
    raise SystemExit(code)

def run(cmd: list[str], *, capture: bool = False) -> subprocess.CompletedProcess:
    try:
        return subprocess.run(cmd, cwd=ROOT, text=True, check=False,
                              stdout=subprocess.PIPE if capture else None,
                              stderr=subprocess.PIPE if capture else None)
    except FileNotFoundError:
        stop(f"Required command not found: {cmd[0]}. Install it and run setup again.")

def main() -> None:
    print("Staff Sales Auction Portal — local setup")
    print(f"Project folder: {ROOT}")
    print("\n[1/5] Checking prerequisites...")

    if sys.version_info < (3, 8):
        stop("Python 3.8 or newer is required.")
    try:
        sqlite3.sqlite_version
    except Exception as exc:
        stop(f"Python SQLite support is unavailable: {exc}")
    print(f"Python: {sys.version.split()[0]} — OK")
    print(f"Python SQLite: {sqlite3.sqlite_version} — OK")

    php = shutil.which("php")
    if not php:
        stop("PHP CLI was not found. Install PHP CLI before continuing.")
    check = run([php, "-r", "echo PHP_VERSION, \"\\n\"; foreach (['pdo','pdo_sqlite','json','mbstring','fileinfo'] as $e) echo $e, ':', extension_loaded($e) ? 'OK' : 'MISSING', \"\\n\";"], capture=True)
    if check.returncode != 0:
        stop("PHP CLI could not run. " + (check.stderr or check.stdout or "").strip())
    print(check.stdout.strip())
    missing = [ext for ext in REQUIRED_EXTENSIONS if f"{ext}:OK" not in check.stdout]
    if missing:
        stop("Install/enable these PHP extensions, then retry: " + ", ".join(missing))

    for item in (CONFIG_EXAMPLE, SCHEMA, ROOT / "app" / "bootstrap.php", ROOT / "tools" / "create_admin.php"):
        if not item.is_file():
            stop(f"Required project file is missing: {item.relative_to(ROOT)}")

    print("\n[2/5] Checking/creating local folders...")
    for folder in (ROOT / "storage" / "database", ROOT / "storage" / "logs",
                   ROOT / "storage" / "item-images", ROOT / "storage" / "sessions"):
        folder.mkdir(parents=True, exist_ok=True)
        if not os.access(folder, os.W_OK):
            stop(f"Folder is not writable by this account: {folder}. Fix permissions and retry.")
        print(f"{folder.relative_to(ROOT)} — writable")

    if CONFIG_LOCAL.exists():
        print("Local configuration already exists — leaving it unchanged.")
    else:
        shutil.copy2(CONFIG_EXAMPLE, CONFIG_LOCAL)
        print("Created config/local.php from the example configuration.")

    # Read the effective database path from the PHP config, rather than assuming it.
    path_check = run([php, "-r", "$c=require 'config/local.php'; if (($c['db']['driver'] ?? '') !== 'sqlite') { fwrite(STDERR, 'SQLite driver required'); exit(2); } echo $c['db']['path'] ?? '';"], capture=True)
    if path_check.returncode != 0 or not path_check.stdout.strip():
        stop("config/local.php must use the SQLite driver and define db.path. Review the local config.")
    db_path = Path(path_check.stdout.strip())
    if not db_path.is_absolute():
        db_path = (ROOT / db_path).resolve()
    if not db_path.parent.exists():
        db_path.parent.mkdir(parents=True, exist_ok=True)
    if not os.access(db_path.parent, os.W_OK):
        stop(f"Database folder is not writable: {db_path.parent}")

    print("\n[3/5] Creating/verifying SQLite schema...")
    try:
        connection = sqlite3.connect(db_path, timeout=5)
        connection.execute("PRAGMA foreign_keys = ON")
        existing = {row[0] for row in connection.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")}
        if not existing:
            connection.executescript(SCHEMA.read_text(encoding="utf-8"))
            connection.commit()
            print("Applied initial schema to a new/empty database.")
        elif EXPECTED_TABLES.issubset(existing):
            print("Expected tables already exist — schema was not re-applied.")
        else:
            missing_tables = sorted(EXPECTED_TABLES - existing)
            stop("Database has a partial/unrecognised schema; it was not changed. Missing tables: " + ", ".join(missing_tables))
        found = {row[0] for row in connection.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")}
        if not EXPECTED_TABLES.issubset(found):
            stop("Schema verification failed. Database was not reset or deleted.")
        fk = connection.execute("PRAGMA foreign_key_check").fetchall()
        if fk:
            stop("Foreign-key integrity check reported a problem.")
        connection.close()
        # Restrict local database access on Unix-like systems; configure the
        # web-server account/group to match these permissions during deployment.
        if os.name != "nt":
            os.chmod(db_path.parent, 0o770)
            os.chmod(db_path, 0o660)
    except sqlite3.Error as exc:
        stop(f"SQLite setup failed: {exc}")
    print(f"Database file: {db_path}")
    print(f"Tables verified: {len(EXPECTED_TABLES)}")

    print("\n[4/5] Checking whether an administrator already exists...")
    check_admin = run([php, "-r", "$c=require 'config/local.php'; $p=new PDO('sqlite:'.$c['db']['path']); echo (int)$p->query('SELECT COUNT(*) FROM users WHERE role=\'admin\'')->fetchColumn();"], capture=True)
    if check_admin.returncode != 0:
        stop("Could not inspect administrator accounts. " + (check_admin.stderr or "").strip())
    if check_admin.stdout.strip() == "0":
        print("No administrator account exists. Starting the secure first-admin wizard.")
        admin = run([php, "tools/create_admin.php"])
        if admin.returncode != 0:
            stop("Administrator setup did not complete. Fix the issue and run: php tools/create_admin.php")
    else:
        print("An administrator already exists — no account was created or changed.")

    print("\n[5/5] Final environment check...")
    final = run([php, "tools/check_environment.php"])
    if final.returncode != 0:
        stop("The final check did not pass. Review the output above.")
    print("\nSetup complete. No default password was created.")
    print("Next: configure your web server document root to this project's public/ directory.")
    print("Important: this is development release v0.9 beta. Run php tools/apply_migrations.php after setup/copying files, then configure the auction finalisation job before controlled bidding tests.")

if __name__ == "__main__":
    main()
