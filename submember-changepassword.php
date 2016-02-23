<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($sub_member_new_password)
{
	if ($sub_member_new_password != $sub_member_new_password2)
	{
		echo "<font color=red>The two passwords you entered does not match.</font>";
		include ("templates/submember-changepassword.html");
		include("templates/footer.html");
		exit;
	}

$result = $client->call("member_update_password", array($login_info , $sub_member_name , $sub_member_new_password));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

} else {
include ("templates/submember-changepassword.html");
}
include("templates/footer.html");
?>
