<?php
declare(strict_types=1);
$configFile = dirname(__DIR__) . '/config/local.php';
if (!is_file($configFile)) { http_response_code(503); exit('Copy config/config.example.php to config/local.php and configure the database.'); }
$config = require $configFile;
date_default_timezone_set((string)($config['timezone'] ?? 'Africa/Windhoek'));
if (PHP_SAPI !== 'cli' && session_status() !== PHP_SESSION_ACTIVE) {
    ini_set('session.use_strict_mode', '1');
    ini_set('session.use_only_cookies', '1');
    ini_set('session.cookie_httponly', '1');
    ini_set('session.cookie_secure', !empty($config['session_secure']) ? '1' : '0');
    ini_set('session.cookie_samesite', 'Lax');
    session_name('staff_sales_session');
    session_start();
}
function app_config(?string $key = null): mixed { global $config; return $key === null ? $config : ($config[$key] ?? null); }
function db(): PDO {
    static $pdo = null;
    if ($pdo instanceof PDO) return $pdo;
    $s = (array)app_config('db');
    $dsn = sprintf('mysql:host=%s;port=%d;dbname=%s;charset=%s', $s['host'] ?? '127.0.0.1', (int)($s['port'] ?? 3306), $s['database'] ?? '', $s['charset'] ?? 'utf8mb4');
    $pdo = new PDO($dsn, (string)($s['username'] ?? ''), (string)($s['password'] ?? ''), [PDO::ATTR_ERRMODE=>PDO::ERRMODE_EXCEPTION, PDO::ATTR_DEFAULT_FETCH_MODE=>PDO::FETCH_ASSOC, PDO::ATTR_EMULATE_PREPARES=>false, PDO::MYSQL_ATTR_INIT_COMMAND=>"SET time_zone = '+00:00'"]);
    return $pdo;
}
function e(mixed $v): string { return htmlspecialchars((string)$v, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8'); }
function app_url(string $p=''): string { return rtrim((string)app_config('base_path'), '/') . '/' . ltrim($p, '/'); }
function redirect_to(string $p): never { header('Location: ' . app_url($p), true, 303); exit; }
function csrf_token(): string { if (!isset($_SESSION['csrf_token']) || !is_string($_SESSION['csrf_token'])) $_SESSION['csrf_token']=bin2hex(random_bytes(32)); return $_SESSION['csrf_token']; }
function verify_csrf(?string $t): bool { return is_string($t) && isset($_SESSION['csrf_token']) && is_string($_SESSION['csrf_token']) && hash_equals($_SESSION['csrf_token'], $t); }
function current_user(): ?array { return isset($_SESSION['user']) && is_array($_SESSION['user']) ? $_SESSION['user'] : null; }
function require_login(): array { $u=current_user(); if ($u===null) redirect_to('login.php'); return $u; }
function require_role(string ...$roles): array { $u=require_login(); if (!in_array((string)($u['role']??''),$roles,true)) { http_response_code(403); exit('You do not have permission to access this page.'); } return $u; }
function audit_log(?int $actor, string $event, ?string $type=null, ?string $entity=null, array $details=[]): void {
    try {
        $s=db()->prepare('INSERT INTO audit_events (actor_user_id,event_type,entity_type,entity_id,ip_address,user_agent,details_json) VALUES (:actor,:event,:type,:entity,:ip,:agent,:details)');
        $s->execute(['actor'=>$actor,'event'=>$event,'type'=>$type,'entity'=>$entity,'ip'=>substr((string)($_SERVER['REMOTE_ADDR']??''),0,45) ?: null,'agent'=>substr((string)($_SERVER['HTTP_USER_AGENT']??''),0,500) ?: null,'details'=>json_encode($details, JSON_UNESCAPED_SLASHES|JSON_UNESCAPED_UNICODE|JSON_THROW_ON_ERROR)]);
    } catch (Throwable $ex) { error_log('Staff Sales audit write failed: '.$ex->getMessage()); if ((string)app_config('app_env')==='production') throw $ex; }
}
