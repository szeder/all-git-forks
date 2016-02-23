<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($amount)
{
$update_total_paid = "yes";
$note = "Adding funds";
$result = $client->call("member_add_funds", array($login_info, $sub_member_name , $amount , $update_total_paid , $note));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

} else {
include ("templates/submember-addfunds.html");
}
include("templates/footer.html");
?>