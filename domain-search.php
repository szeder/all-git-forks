<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}
if ($search_domain_name)
{
	$params = array($login_info , $search_domain_name);
	$result = $client->call("member_domain_search", $params);
	
	$err = $client->getError();
	if ($err) { 
		include("templates/domain-search.html");
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		$search_result = "ok";
		include("templates/domain-search.html");
		echo "<center><a href=submembers.php>Sub member domains list</a></center>";
	}
} else {
	include("templates/domain-search.html");
	echo "<center><a href=submembers.php>Sub member domains list</a></center>";
}
include("templates/footer.html");
?>