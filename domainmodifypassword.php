<?php
require("config.php"); // Configuration file
include ("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($submit)
{
	if ($domain_new_password != $domain_new_password2)
	{
		echo "<font color=red>The two passwords you entered does not match.</font>";
		include ("templates/domainmodifypassword.html");
		include("templates/footer.html");
		exit;
	}

$domain_details = array();
$domain_details[0] = Array("domain_name" => $domain_name ,
				   "domain_new_password" => $domain_new_password);

$params = array($login_info , $domain_details);
$result = $client->call("domain_modify_password", $params);

	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}

} else {
include ("templates/domainmodifypassword.html");

}
include ("templates/footer.html"); ?>
