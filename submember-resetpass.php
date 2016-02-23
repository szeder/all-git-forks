<?
require("config.php"); // Configuration file
include("templates/header.html");

if (isset($_SESSION['member_name']))
{
	echo "<center><font color=red>You cannot register for an account while you are logged in.</font></center>";
	exit;
}

$result = $client->call("member_reset_password", array($parent_login_info, $sub_member_name , $auth_code));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

include("templates/footer.html");
?>