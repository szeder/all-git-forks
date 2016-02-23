<?
require("config.php"); // Configuration file
include("templates/header.html");

if (isset($_SESSION['member_name']))
{
	echo "<center><font color=red>You cannot register for an account while you are logged in.</font></center>";
	exit;
}

if ($sub_member_full_name)
{
	if ($sub_member_password != $sub_member_password2)
	{
		echo "<font color=red>The two passwords you entered does not match.</font>";
		include ("templates/submember-join.html");
		include("templates/footer.html");
		exit;
	}
$sub_member_telephone_code = 2;
$sub_member_fax_code = 2;

		$result = $client->call("member_register", array($parent_login_info, $sub_member_name1 , $sub_member_password , $sub_member_full_name , $sub_member_org , $sub_member_address1 , $sub_member_address2 , $sub_member_address3 , $sub_member_city , $sub_member_state , $sub_member_country , $sub_member_postalcode , $sub_member_telephone_code , $sub_member_telephone , $sub_member_fax_code , $sub_member_fax , $sub_member_email));
	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	echo "<br>";

	$result2 = $client->call("member_register_domain_reseller", array($parent_login_info, $sub_member_name1));
		$err = $client->getError();
		if ($err) {
			echo "<font color=red>" . $result2['faultactor'] . "</font>";
			echo "<font color=red>" . $result2['faultstring'] . "</font>";
		} else {
			echo $result2['result']['details'];
		}
	}
} else {

include ("templates/submember-join.html");

}
include("templates/footer.html");
?>
