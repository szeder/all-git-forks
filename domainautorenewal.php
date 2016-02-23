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
				   "domain_auto_renewal" => $domain_auto_renewal);

$params = array($login_info , $domain_details);
$result = $client->call("domain_set_auto_renewal", $params);

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
include ("templates/domainautorenewal.html");
}
}
include ("templates/footer.html"); ?>