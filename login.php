<?
require("config.php"); // Configuration file


$member_password2 = md5($member_password);
$member_api_password = md5($member_name . $member_password2);
$member_login_info = array("member_name" => $member_name ,
			"member_api_password" => $member_api_password);

$params = array($member_login_info);
$result = $client->call("member_login", $params);
$err = $client->getError();
if ($err) { 
	include ("templates/header.html");
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
	include ("templates/index.html");
	include ("templates/footer.html");
	exit;
} else {
	if(!strstr($result['result']['details'], "successfully"))
	{  
	include ("templates/header.html");
	echo "<font color=red>" . "Invalid member name and password" . "</font>";
	include ("templates/index.html");
	include ("templates/footer.html");
	exit;
	}
}

$_SESSION['member_name'] = $member_name;
$_SESSION['member_password'] = $member_password;
header("Location: home.php");

?>

