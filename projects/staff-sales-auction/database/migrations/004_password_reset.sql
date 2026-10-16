-- Migration 004: email addresses and single-use self-service password reset tokens.
-- Applied by tools/apply_migrations.php to preserve existing data.
ALTER TABLE users ADD COLUMN email TEXT;
CREATE UNIQUE INDEX idx_users_email_unique ON users(email COLLATE NOCASE) WHERE email IS NOT NULL AND email <> '';
CREATE TABLE password_reset_tokens (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 user_id INTEGER NOT NULL,
 token_hash TEXT NOT NULL UNIQUE,
 expires_at TEXT NOT NULL,
 used_at TEXT,
 created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),
 FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);
CREATE INDEX idx_password_reset_user_expiry ON password_reset_tokens(user_id,expires_at,used_at);
