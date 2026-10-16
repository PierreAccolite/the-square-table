-- Multi-permission access and auditable procurement change approvals.
CREATE TABLE IF NOT EXISTS user_permissions (
 user_id INTEGER NOT NULL,
 permission TEXT NOT NULL CHECK(permission IN ('bidder','procurement','custodian','approver')),
 granted_by INTEGER,
 granted_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),
 PRIMARY KEY(user_id,permission),
 FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE,
 FOREIGN KEY(granted_by) REFERENCES users(id) ON DELETE SET NULL
);
CREATE INDEX IF NOT EXISTS idx_user_permissions_permission ON user_permissions(permission,user_id);
CREATE TABLE IF NOT EXISTS approval_requests (
 id INTEGER PRIMARY KEY AUTOINCREMENT,
 request_type TEXT NOT NULL CHECK(request_type IN ('item_change')),
 entity_type TEXT NOT NULL CHECK(entity_type IN ('auction_item')),
 entity_id INTEGER NOT NULL,
 requested_by INTEGER NOT NULL,
 reason TEXT NOT NULL CHECK(length(reason) BETWEEN 5 AND 2000),
 before_json TEXT NOT NULL,
 after_json TEXT NOT NULL,
 status TEXT NOT NULL DEFAULT 'pending' CHECK(status IN ('pending','approved','rejected','stale')),
 reviewed_by INTEGER,
 review_comment TEXT,
 created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%S','now')),
 reviewed_at TEXT,
 FOREIGN KEY(requested_by) REFERENCES users(id) ON DELETE RESTRICT,
 FOREIGN KEY(reviewed_by) REFERENCES users(id) ON DELETE RESTRICT
);
CREATE INDEX IF NOT EXISTS idx_approval_status_created ON approval_requests(status,created_at);
CREATE INDEX IF NOT EXISTS idx_approval_entity ON approval_requests(entity_type,entity_id);
