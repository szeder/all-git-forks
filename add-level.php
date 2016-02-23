<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($add == "yes") {
	$params = array($login_info);
	$result = $client->call("member_add_level", $params);
	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}
} else {
?>
<br>
Are you sure to add new member level for your sub resellers?
<br>
<a href="add-level.php?add=yes">Yes</a>
<br>
<a href="home.php">No</a>

<?
}
include("templates/footer.html");
?>