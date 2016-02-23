<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}
if ($search_member_name)
{
	$params = array($login_info , $search_member_name);
	$result = $client->call("member_member_search", $params);
	
	$err = $client->getError();
	if ($err) { 
		include("templates/submember-search.html");
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		$search_result = "ok";
		include("templates/submember-search.html");
	}
} else {
	include("templates/submember-search.html");
}
include("templates/footer.html");
?>
