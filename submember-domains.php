<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

$params = array($login_info , $sub_member_name , $startYear , $startMonth , $startDay , $toYear , $toMonth , $toDay , $search_domain_name , $locking , $status , $auto_renewal , $order_by , $number_of_results , $page_number);
$result = $client->call("member_domain_list", $params);

$err = $client->getError();
if ($err) { 
	include("templates/submember-domains.html");
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	include("templates/submember-domains.html");
	echo "<center><a href=submembers.php>Sub member domains list</a></center>";
}
include("templates/footer.html");
?>