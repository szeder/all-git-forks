<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}
if (!$sub_member_name)
{
	$sub_member_name = $member_name;
}
$params = array($login_info , $sub_member_name , $startYear , $startMonth , $startDay , $toYear , $toMonth , $toDay , $search_domain_name , $locking , $status , $auto_renewal , $order_by , $number_of_results , $page_number);
$result = $client->call("sub_member_domain_list", $params);

$err = $client->getError();
if ($err) { 
	include("templates/submember-all-domains.html");
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	include("templates/submember-all-domains.html");
}
include("templates/footer.html");
?>
