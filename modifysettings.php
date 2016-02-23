<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($site_title)
{
$contact_me = "yes";
$result = $client->call("member_update_settings", array($login_info , $member_name , $site_title , $site_url , $contact_email , $contact_gsm , $balance_warning , $sms_balance_warning , $dns1 , $dns2 , $dns3 , $dns4 , $dns5 , $login_email , $contact_me));
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
include ("templates/modifysettings.html");
}

}
include("templates/footer.html");
?>