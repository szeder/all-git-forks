<?
include("config.php");
$old_user = $domain_name; //test if user *were* logged in
$result = session_unregister("member_name");
session_destroy();
header("Location: index.php");
?>
<meta http-equiv="refresh" content="0;URL=index.php">