<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($sub_member_new_status)
{
$result = $client->call("member_set_special_price", array($login_info , $sub_member_name , $sub_member_new_status));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

} else {
$params = array($login_info , $sub_member_name);
$result = $client->call("member_show_details", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
$member_details = $result['result']['details'];
include ("templates/submember-specialprice.html");
}
}
include("templates/footer.html");
?>