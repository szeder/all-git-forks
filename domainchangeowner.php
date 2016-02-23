<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($sub_member_name)
{
$domain_details = array();
$domain_details[0] = Array("domain_name" => $domain_name,
			"sub_member_name" => $sub_member_name);

$result = $client->call("domain_change_owner", array($login_info , $domain_details));

	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}

} else {
	include ("templates/domainchangeowner.html");
}
include("templates/footer.html");
?>