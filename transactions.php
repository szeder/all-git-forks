<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

$params = array($login_info , $member_name , $startYear , $startMonth , $startDay , $toYear , $toMonth , $toDay , $type , $number_of_results , $page_number);
$result = $client->call("member_transactions_list", $params);

$err = $client->getError();
if ($err) { 
	include("templates/transactions.html");
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	include("templates/transactions.html");
}
include("templates/footer.html");
?>