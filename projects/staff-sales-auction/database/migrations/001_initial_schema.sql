-- Staff Sales Auction Portal v0.1.0. Apply once to a new empty database.
SET NAMES utf8mb4;
SET time_zone = '+00:00';

CREATE TABLE users (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
 username VARCHAR(80) NOT NULL,
 display_name VARCHAR(160) NOT NULL,
 employee_number VARCHAR(80) NULL,
 password_hash VARCHAR(255) NOT NULL,
 role ENUM('employee','admin','auditor') NOT NULL DEFAULT 'employee',
 status ENUM('active','disabled') NOT NULL DEFAULT 'active',
 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
 last_login_at DATETIME NULL,
 PRIMARY KEY (id), UNIQUE KEY uq_users_username (username),
 UNIQUE KEY uq_users_employee_number (employee_number),
 KEY idx_users_status_role (status, role)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE auctions (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
 title VARCHAR(180) NOT NULL, description TEXT NULL,
 status ENUM('draft','scheduled','open','closed','cancelled') NOT NULL DEFAULT 'draft',
 starts_at DATETIME NULL, ends_at DATETIME NULL,
 anti_sniping_window_seconds SMALLINT UNSIGNED NOT NULL DEFAULT 120,
 anti_sniping_extension_seconds SMALLINT UNSIGNED NOT NULL DEFAULT 120,
 created_by BIGINT UNSIGNED NOT NULL,
 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
 closed_at DATETIME NULL, PRIMARY KEY (id),
 KEY idx_auctions_status_dates (status, starts_at, ends_at),
 CONSTRAINT fk_auctions_created_by FOREIGN KEY (created_by) REFERENCES users(id) ON DELETE RESTRICT,
 CONSTRAINT chk_auctions_extension_window CHECK (anti_sniping_window_seconds > 0 AND anti_sniping_extension_seconds > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE auction_items (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, auction_id BIGINT UNSIGNED NOT NULL,
 lot_number VARCHAR(40) NOT NULL, title VARCHAR(180) NOT NULL, description TEXT NULL,
 condition_notes TEXT NULL, image_path VARCHAR(500) NULL,
 opening_price DECIMAL(12,2) NOT NULL, minimum_increment DECIMAL(12,2) NOT NULL DEFAULT 10.00,
 status ENUM('draft','published','withdrawn','awarded') NOT NULL DEFAULT 'draft',
 created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
 PRIMARY KEY (id), UNIQUE KEY uq_auction_lot (auction_id, lot_number),
 KEY idx_auction_items_status (auction_id, status),
 CONSTRAINT fk_auction_items_auction FOREIGN KEY (auction_id) REFERENCES auctions(id) ON DELETE RESTRICT,
 CONSTRAINT chk_auction_item_prices CHECK (opening_price >= 0 AND minimum_increment > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE bids (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, auction_item_id BIGINT UNSIGNED NOT NULL,
 bidder_user_id BIGINT UNSIGNED NOT NULL, amount DECIMAL(12,2) NOT NULL,
 client_request_id CHAR(36) NOT NULL, placed_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
 PRIMARY KEY (id), UNIQUE KEY uq_bids_request_id (client_request_id),
 KEY idx_bids_item_amount_time (auction_item_id, amount, placed_at, id),
 KEY idx_bids_bidder_time (bidder_user_id, placed_at),
 CONSTRAINT fk_bids_item FOREIGN KEY (auction_item_id) REFERENCES auction_items(id) ON DELETE RESTRICT,
 CONSTRAINT fk_bids_bidder FOREIGN KEY (bidder_user_id) REFERENCES users(id) ON DELETE RESTRICT,
 CONSTRAINT chk_bids_positive_amount CHECK (amount > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE audit_events (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, actor_user_id BIGINT UNSIGNED NULL,
 event_type VARCHAR(100) NOT NULL, entity_type VARCHAR(80) NULL, entity_id VARCHAR(100) NULL,
 ip_address VARCHAR(45) NULL, user_agent VARCHAR(500) NULL, details_json JSON NULL,
 created_at DATETIME(6) NOT NULL DEFAULT CURRENT_TIMESTAMP(6),
 PRIMARY KEY (id), KEY idx_audit_created_at (created_at),
 KEY idx_audit_actor_time (actor_user_id, created_at), KEY idx_audit_entity (entity_type, entity_id),
 CONSTRAINT fk_audit_actor FOREIGN KEY (actor_user_id) REFERENCES users(id) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE login_attempts (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, username_key VARCHAR(80) NOT NULL,
 ip_address VARCHAR(45) NOT NULL DEFAULT '', attempted_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (id), KEY idx_login_attempts_lookup (username_key, ip_address, attempted_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE notifications (
 id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT, user_id BIGINT UNSIGNED NOT NULL,
 notification_type VARCHAR(80) NOT NULL, title VARCHAR(180) NOT NULL, message TEXT NOT NULL,
 read_at DATETIME NULL, created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
 PRIMARY KEY (id), KEY idx_notifications_user_read (user_id, read_at, created_at),
 CONSTRAINT fk_notifications_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
