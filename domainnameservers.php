<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

$params = array($login_info, $domain_name);
$result = $client->call("domain_nameservers_list", $params);
$domain_details = $result['result']['details'];
$err = $client->getError();
if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	include("templates/domainnameservers.html");
}
include("templates/footer.html");
?>