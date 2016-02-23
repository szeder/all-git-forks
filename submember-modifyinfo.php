<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($sub_member_full_name)
{
$sub_member_telephone_code = 2;
$sub_member_fax_code = 2;

$result = $client->call("member_update_info", array($login_info , $sub_member_name , $sub_member_full_name , $sub_member_org , $sub_member_address1 , $sub_member_address2 , $sub_member_address3 , $sub_member_city , $sub_member_state , $sub_member_country , $sub_member_postalcode , $sub_member_telephone_code , $sub_member_telephone , $sub_member_fax_code , $sub_member_fax , $sub_member_email));
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
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
$member_details = $result['result']['details'];
include ("templates/submember-modifyinfo.html");
}
}
include("templates/footer.html");
?>