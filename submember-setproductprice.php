<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

$result = $client->call("member_set_product_price", array($login_info , $sub_member_name , $product_type , $product_new_price));
$err = $client->getError(); 
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

include("templates/footer.html");
?>