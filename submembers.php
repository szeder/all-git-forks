<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}
$params = array($login_info , $member_name , $startYear , $startMonth , $startDay , $toYear , $toMonth , $toDay , $search_member_name , $flag , $member_level , $special_price , $balance , $total_paid , $order_by , $number_of_records , $page_number);
$result = $client->call("member_member_list", $params);

$err = $client->getError();
if ($err) { 
	include("templates/submembers.html");
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	include("templates/submembers.html");
}
include("templates/footer.html");
?>
