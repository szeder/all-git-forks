<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($member_new_password)
{
	if ($member_new_password != $member_new_password2)
	{
		echo "<font color=red>The two passwords you entered does not match.</font>";
		include ("templates/changepassword.html");
		include("templates/footer.html");
		exit;
	}

	$result = $client->call("member_update_password", array($login_info , $member_name , $member_new_password));
	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}
} else {
include ("templates/changepassword.html");
}
include("templates/footer.html");
?>