<?
require("config.php"); // Configuration file
include ("templates/header.html");
$params = array($domain_name , $domain_password);
if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($newIp)
{
$domain_details = array();
$domain_details[0] = Array("domain_name" => $domain_name ,
				   "nameserver" => $nameserver,
				   "oldIp" => $oldIp,
				   "newIp" => $newIp);

$params = array($login_info , $domain_details);
$result = $client->call("domain_modify_nameserver", $params);

	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}

} else {
$result = $client->call("domain_nameserver_details", array($login_info, $domain_name , $nameserver));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
$domain_details = $result['result']['details'];
include ("templates/domainmodifynameserver.html");
}
}
include ("templates/footer.html"); ?>