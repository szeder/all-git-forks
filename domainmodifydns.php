<?
require("config.php"); // Configuration file
include ("templates/header.html");
$params = array($domain_name , $domain_password);
if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($submit)
{
$domain_details = array();
$domain_details[0] = Array("domain_name" => $domain_name ,
				   "dns1" => $dns1,
				   "dns2" => $dns2,
				   "dns3" => $dns3,
				   "dns4" => $dns4,
				   "dns5" => $dns5);

$result = $client->call("domain_modify_dns", array($login_info, $domain_details));

	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}

} else {

$params = array($login_info, $domain_name);
$result = $client->call("domain_show_details", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
$domain_details = $result['result']['details'];
include ("templates/domainmodifydns.html");
}
}
include ("templates/footer.html"); ?>
