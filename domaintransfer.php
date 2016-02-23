<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

$params = array($login_info , $member_name , $startYear , $startMonth , $startDay , $toYear , $toMonth , $toDay , $search_domain_name , $order_by , $number_of_results , $page_number);
$result = $client->call("member_domain_transfer_list", $params);

$err = $client->getError();
if ($err) { 
	include("templates/domaintransfer.html");
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	include("templates/domaintransfer.html");
}
include("templates/footer.html");
?>