<?php
declare(strict_types=1);
require dirname(__DIR__,2).'/app/bootstrap.php';
$user=require_role('admin');
$error='';$summary=null;$created=[];$skipped=[];
if($_SERVER['REQUEST_METHOD']==='POST'){
 if(!verify_csrf(isset($_POST['csrf_token'])?(string)$_POST['csrf_token']:null)){http_response_code(400);exit('Invalid request token.');}
 $file=$_FILES['users_csv']??null;
 if(!is_array($file)||($file['error']??UPLOAD_ERR_NO_FILE)!==UPLOAD_ERR_OK)$error='Choose a valid CSV file to upload.';
 elseif((int)($file['size']??0)>2*1024*1024)$error='CSV files must be 2 MB or smaller.';
 elseif(!is_uploaded_file((string)($file['tmp_name']??'')))$error='The uploaded file could not be verified.';
 else{
  $fh=fopen((string)$file['tmp_name'],'rb');
  if(!$fh)$error='Could not read the CSV file.';
  else{
   $header=fgetcsv($fh);
   $required=['username','display_name','employee_number','role','initial_password'];
   $normal=array_map(static fn($v)=>strtolower(trim((string)$v)),is_array($header)?$header:[]);
   if(!$header||array_diff($required,$normal))$error='CSV header must contain: username, display_name, employee_number, role, initial_password.';
   else{
    $idx=array_flip($normal);$line=1;$seen=[];$pdo=db();
    while(($row=fgetcsv($fh))!==false){
     $line++;if(count($row)===1&&trim((string)$row[0])==='')continue;
     $v=[];foreach($required as $key)$v[$key]=trim((string)($row[$idx[$key]]??''));
     $why='';
     if(!preg_match('/^[A-Za-z0-9._-]{3,80}$/',$v['username']))$why='invalid username';
     elseif($v['display_name']===''||mb_strlen($v['display_name'])>160)$why='display name required (max 160)';
     elseif(mb_strlen($v['employee_number'])>80)$why='employee number too long';
     elseif(!in_array($v['role'],['employee','admin','auditor'],true))$why='role must be employee, admin or auditor';
     elseif(strlen($v['initial_password'])<12)$why='initial password must be at least 12 characters';
     elseif(isset($seen[strtolower($v['username'])]))$why='duplicate username in CSV';
     else $seen[strtolower($v['username'])]=true;
     if($why!==''){$skipped[]=['line'=>$line,'username'=>$v['username'],'reason'=>$why];continue;}
     try{
      $s=$pdo->prepare('INSERT INTO users(username,display_name,employee_number,password_hash,role,status) VALUES(:u,:d,:e,:p,:r,\'active\')');
      $s->execute(['u'=>$v['username'],'d'=>$v['display_name'],'e'=>$v['employee_number']!==''?$v['employee_number']:null,'p'=>password_hash($v['initial_password'],PASSWORD_DEFAULT),'r'=>$v['role']]);
      $id=(int)$pdo->lastInsertId();audit_log((int)$user['id'],'user.bulk_created','user',(string)$id,['role'=>$v['role'],'source_line'=>$line]);$created[]=$v['username'];
     }catch(PDOException $ex){if(str_contains(strtolower($ex->getMessage()),'unique'))$skipped[]=['line'=>$line,'username'=>$v['username'],'reason'=>'username or employee number already exists'];else throw $ex;}
    }
    fclose($fh);$summary=['created'=>count($created),'skipped'=>count($skipped)];
   }
  }
 }
}
require dirname(__DIR__,2).'/app/layout.php';render_page_start('Bulk user upload','users',$user);
?>
<section class="welcome"><div><p class="eyebrow">ACCOUNT ADMINISTRATION</p><h1>Bulk user upload</h1><p class="muted">Create multiple staff accounts from a controlled CSV template.</p></div><div class="welcome-icon">⇧</div></section>
<div class="notice"><span class="notice-dot"></span><div><strong>Handle the CSV securely</strong><p>Each row must include an initial password of at least 12 characters. Use a secure transfer method, do not email the file, and delete it after import. Passwords are hashed before storage.</p></div></div>
<?php if($error!==''): ?><div class="alert" role="alert"><?= e($error) ?></div><?php endif; ?>
<?php if($summary): ?><div class="notice"><span class="notice-dot"></span><div><strong>Import finished</strong><p><?= (int)$summary['created'] ?> account(s) created; <?= (int)$summary['skipped'] ?> row(s) skipped. Existing accounts were not changed.</p></div></div><?php if($created): ?><section class="content-panel"><h2>Created accounts</h2><ul><?php foreach($created as $name): ?><li><?= e($name) ?></li><?php endforeach; ?></ul></section><?php endif; ?><?php if($skipped): ?><section class="content-panel"><h2>Rows requiring attention</h2><div class="table-wrap"><table class="data-table"><thead><tr><th>CSV line</th><th>Username</th><th>Reason</th></tr></thead><tbody><?php foreach($skipped as $s): ?><tr><td><?= (int)$s['line'] ?></td><td><?= e($s['username']) ?></td><td><?= e($s['reason']) ?></td></tr><?php endforeach; ?></tbody></table></div></section><?php endif; ?><?php endif; ?>
<section class="content-panel"><div class="section-heading"><div><p class="eyebrow">CSV IMPORT</p><h2>Select a user file</h2></div></div><p class="muted">Required columns: <code>username,display_name,employee_number,role,initial_password</code>. Supported roles: <code>employee</code>, <code>admin</code>, <code>auditor</code>. UTF-8 CSV, maximum 2 MB.</p><p><button class="button button-secondary" type="button" onclick="window.location.href='<?= e(app_url('admin/user-import-template.php')) ?>'">Download CSV template</button></p><form method="post" enctype="multipart/form-data" class="admin-form"><input type="hidden" name="csrf_token" value="<?= e(csrf_token()) ?>"><label for="users_csv">User CSV file</label><input id="users_csv" name="users_csv" type="file" accept=".csv,text/csv" required><button class="button button-primary" type="submit">Import users →</button></form></section>
<p><a class="text-link" href="<?= e(app_url('admin/users.php')) ?>">← Return to user management</a></p>
<?php render_page_end(); ?>