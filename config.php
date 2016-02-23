<?
require_once("nusoap.php");
$client = new soapclient("http://www.DevNOC.com/brain/");

ob_start();
session_start(); 


//FILL IN YOUR DETAILS BELOW
//Your member name
$parent_member_name = "";
//Your member password
$parent_member_password = "";
//Your contact email
$parent_contact_email = "";
//Your script URL without / at the end e.g. http://www.site.com/domains
$parent_script_url = "";


//DON`T EDIT ANYTHING BELOW THIS LINE
$parent_member_password2 = md5($parent_member_password);
$parent_member_api_password = md5($parent_member_name . $parent_member_password2);
$parent_login_info = array("member_name" => $parent_member_name ,
				"member_api_password" => $parent_member_api_password);

if ($member_name & $member_password)
{
	$member_password2 = md5($_SESSION['member_password']);
	$member_api_password = md5($_SESSION['member_name'] . $member_password2);
	$login_info = array("member_name" => $_SESSION['member_name'] ,
				"member_api_password" => $member_api_password);
}
$version = "2.0";
?>
