<?
require("config.php"); // Configuration file
include ("templates/header.html");
$params = array($domain_name , $domain_password);
if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($ipaddr)
{
$domain_details = array();
$domain_details[0] = Array("domain_name" => $domain_name ,
				   "nameserver" => $nameserver,
				   "ipaddr" => $ipaddr);

$params = array($login_info , $domain_details);
$result = $client->call("domain_delete_nameserver", $params);

	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}

} else {

echo "error";

}
include ("templates/footer.html"); ?>