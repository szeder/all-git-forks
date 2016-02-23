<?
require("config.php"); // Configuration file
include("templates/header.html");

if ($sub_member_name)
{


	$result = $client->call("member_register_domain_reseller", array($parent_login_info, $sub_member_name));
		$err = $client->getError();
		if ($err) {
			echo "<font color=red>" . $result['faultactor'] . "</font>";
			echo "<font color=red>" . $result['faultstring'] . "</font>";
		} else {
			echo $result['result']['details'];
		}

} else {
	include("templates/submember-domainreseller.html");
}
include("templates/footer.html");
?>
