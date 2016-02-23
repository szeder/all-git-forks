<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($member_full_name)
{
$member_telephone_code = 2;
$member_fax_code = 2;

$result = $client->call("member_update_info", array($login_info , $member_name , $member_full_name , $member_org , $member_address1 , $member_address2 , $member_address3 , $member_city , $member_state , $member_country , $member_postalcode , $member_telephone_code , $member_telephone , $member_fax_code , $member_fax , $member_email));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

} else {
$params = array($login_info , $member_name);
$result = $client->call("member_show_details", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
$member_details = $result['result']['details'];
include ("templates/modifyinfo.html");
}

}
include("templates/footer.html");
?>