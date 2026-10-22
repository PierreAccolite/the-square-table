#!/usr/bin/env python3
"""Safely add manager_user_id to an existing Staff Sales Auction SQLite database."""
from __future__ import annotations
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "config" / "local.php"

def configured_database() -> Path:
    if not CONFIG.is_file():
        raise SystemExit(f"Missing {CONFIG}. Run this script from an installed project with config/local.php present.")
    php = shutil.which("php")
    if not php:
        raise SystemExit("PHP CLI is required to read config/local.php safely.")
    code = '$c=require $argv[1]; echo $c["db"]["path"] ?? "";'
    result = subprocess.run([php, "-r", code, str(CONFIG)], text=True, capture_output=True)
    if result.returncode != 0 or not result.stdout.strip():
        raise SystemExit("Could not read the SQLite path from config/local.php.")
    return Path(result.stdout.strip()).expanduser().resolve()

def main() -> int:
    import sqlite3
    db_path = configured_database()
    if not db_path.is_file():
        raise SystemExit(f"SQLite database not found: {db_path}")
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    backup = db_path.with_name(db_path.name + f".pre-manager-migration-{stamp}.bak")
    shutil.copy2(db_path, backup)
    print(f"Backup created: {backup}")
    try:
        con = sqlite3.connect(str(db_path))
        con.execute("PRAGMA foreign_keys=ON")
        columns = {row[1] for row in con.execute("PRAGMA table_info(users)")}
        if "manager_user_id" not in columns:
            con.execute("ALTER TABLE users ADD COLUMN manager_user_id INTEGER REFERENCES users(id) ON DELETE SET NULL")
            print("Added users.manager_user_id.")
        else:
            print("users.manager_user_id already exists; no column change needed.")
        con.execute("CREATE INDEX IF NOT EXISTS idx_users_manager_user_id ON users(manager_user_id)")
        con.commit()
        check = {row[1] for row in con.execute("PRAGMA table_info(users)")}
        if "manager_user_id" not in check:
            raise RuntimeError("Migration verification failed.")
        print("Migration verified. Existing user records were preserved.")
        con.close()
        return 0
    except Exception:
        try:
            con.rollback()
            con.close()
        except Exception:
            pass
        raise

if __name__ == "__main__":
    raise SystemExit(main())
