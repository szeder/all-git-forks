<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($domain_name2)
{
$domain_details = array();
$domain_details[0] = Array("domain_name" => $domain_name2 ,
				   "domain_years" => $domain_years);

$result = $client->call("domain_renew", array($login_info , $domain_details));

$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

} else {
include("templates/domainrenew.html");

}
include("templates/footer.html");
?>
