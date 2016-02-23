<?
require("config.php"); // Configuration file
include("templates/header.html");

if (isset($_SESSION['member_name']))
{
	echo "<center><font color=red>You cannot access this page while you are logged in.</font></center>";
	exit;
}
if ($sub_member_name)
{

$url = $parent_script_url . "/submember-resetpass.php";
$result = $client->call("member_forgot_password", array($parent_login_info, $sub_member_name , $url));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}
} else {
include ("templates/submember-forgotpass.html");
}

include("templates/footer.html");
?>