<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}
?>
[<a href=mypricelist.php?action=register>Domain registration</a>]
[<a href=mypricelist.php?action=transfer>Domain transfer</a>]
[<a href=mypricelist.php?action=renewal>Domain renewal</a>]
<br>
<?
if ($action == "register")
{
echo "<b>Domain registration pricing</b><br>";

$product_type = "domcom_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "COM domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domnet_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "NET domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domorg_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "ORG domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dominfo_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "INFO domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dombiz_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "BIZ domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domus_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "US domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domname_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "NAME domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domin_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "IN domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dommobi_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "MOBI domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dombz_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "BZ domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dommn_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "MN domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domcc_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "CC domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domtv_register";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "TV domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}


}


if ($action == "transfer")
{
echo "<b>Domain transfer pricing</b><br>";

$product_type = "domcom_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "COM domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domnet_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "NET domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domorg_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "ORG domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dominfo_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "INFO domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dombiz_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "BIZ domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domus_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "US domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domname_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "NAME domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domin_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "IN domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dommobi_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "MOBI domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dombz_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "BZ domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dommn_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "MN domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domcc_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "CC domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domtv_transfer";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "TV domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




}


if ($action == "renewal")
{
echo "<b>Domain renewal pricing</b><br>";


$product_type = "domcom_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "COM domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domnet_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "NET domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domorg_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "ORG domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dominfo_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "INFO domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dombiz_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "BIZ domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domus_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "US domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domname_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "NAME domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domin_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "IN domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dommobi_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "MOBI domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dombz_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "BZ domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "dommn_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "MN domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domcc_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "CC domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}




$product_type = "domtv_renewal";
$params = array($login_info , $member_name , $product_type , 1);
$result = $client->call("member_product_price", $params);
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
echo "TV domain names for 1 year is $" . $result['result']['details'];
echo "<br>";
}


}

include("templates/footer.html");
?>