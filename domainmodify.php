<?php
require("config.php"); // Configuration file
include ("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($submit)
{
$contact_details = array();
$contact_details[0] = Array("domain_name" => $domain_name,
			"contact_type" => "registrant",
			"contact_name" => $registrant ,
			"contact_org" => $r_org,
			"contact_address1" => $r_address1,
			"contact_address2" => $r_address2,
			"contact_address3" => $r_address3,
			"contact_city" => $r_city,
			"contact_state" => $r_state,
			"contact_country" => $r_country,
			"contact_postalcode" => $r_postalcode,
			"contact_telephone_code" => "2",
			"contact_telephone" => $r_telephone,
			"contact_fax_code" => "2",
			"contact_fax" => $r_fax,
			"contact_email" => $r_email);

$contact_details[1] = Array("domain_name" => $domain_name,
			"contact_type" => "admin",
			"contact_name" => $administrator ,
			"contact_org" => $a_org,
			"contact_address1" => $a_address1,
			"contact_address2" => $a_address2,
			"contact_address3" => $a_address3,
			"contact_city" => $a_city,
			"contact_state" => $a_state,
			"contact_country" => $a_country,
			"contact_postalcode" => $a_postalcode,
			"contact_telephone_code" => "2",
			"contact_telephone" => $a_telephone,
			"contact_fax_code" => "2",
			"contact_fax" => $a_fax,
			"contact_email" => $a_email);

$contact_details[2] = Array("domain_name" => $domain_name,
			"contact_type" => "tech",
			"contact_name" => $technical ,
			"contact_org" => $t_org,
			"contact_address1" => $t_address1,
			"contact_address2" => $t_address2,
			"contact_address3" => $t_address3,
			"contact_city" => $t_city,
			"contact_state" => $t_state,
			"contact_country" => $t_country,
			"contact_postalcode" => $t_postalcode,
			"contact_telephone_code" => "2",
			"contact_telephone" => $t_telephone,
			"contact_fax_code" => "2",
			"contact_fax" => $t_fax,
			"contact_email" => $t_email);

$contact_details[3] = Array("domain_name" => $domain_name,
			"contact_type" => "billing",
			"contact_name" => $billing ,
			"contact_org" => $b_org,
			"contact_address1" => $b_address1,
			"contact_address2" => $b_address2,
			"contact_address3" => $b_address3,
			"contact_city" => $b_city,
			"contact_state" => $b_state,
			"contact_country" => $b_country,
			"contact_postalcode" => $b_postalcode,
			"contact_telephone_code" => "2",
			"contact_telephone" => $b_telephone,
			"contact_fax_code" => "2",
			"contact_fax" => $b_fax,
			"contact_email" => $b_email);

$params = array($login_info , $contact_details);
$result = $client->call("contact_modify", $params);


	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
	} else {
		echo $result['result']['details'];
	}

} else {

$params = array($login_info, $domain_name);
$result = $client->call("domain_show_contact", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
$domain_details = $result['result']['details'];
include ("templates/domainmodify.html");
}
}
include ("templates/footer.html"); ?>
