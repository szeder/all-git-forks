<?
require("config.php"); // Configuration file
include ("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if ($parent_member_name == $_SESSION['member_name'])
{
	$file = fopen ("http://www.hexaserv.com/HS/version.txt", "r");
	while (!feof ($file))
	{
		$line = fgets ($file, 1024);
	}
	if ($line != $version)
	{
		echo "<center><b><font color=red>New version is available from this script. <a 	href=http://www.hexaserv.com/DomainManager.zip>Download it NOW</a> </font></b></center>";
	}
}
?>
<br>
<table border="2" cellpadding="0" cellspacing="0" style="border-collapse: collapse" bordercolor="#111111" width="95%" id="AutoNumber1">
  <tr>
    <td width="50%">&nbsp;      
    <table width="100%" border="0" cellspacing="0" cellpadding="0" bordercolor="#111111" height="105">
        <tr> 
          <td align="center" width="100%">
<?
echo "<b>Welcome " . $member_name . "</b>";
echo "<br><br>";
$params = array($login_info , $member_name);
$result = $client->call("member_balance", $params);
	$err = $client->getError();
	if ($err) { 
		echo $result['faultactor'];
		echo $result['faultstring'];
	} else {
		echo "Your balance is " . $result['result']['details'];
	}



echo "<br><br>";
?>
          </td>
        </tr>
        <tr>
          <td width="100%">
		<b>Quick links </b><br>
            <a href=domains.php>Domain list</a> <br>
            <a href=domainchangeowner.php>Move domain</a> <br>
            <a href=registerdomain.php>Register domain</a> <br>
            <a href=transferdomain.php>Transfer domain</a><br>
            <a href=submembers.php>Sub members</a> <br>
          </td>
        </tr>
      </table>
</td>
    <td width="50%"><? include("checkdomain.php"); ?></td>
  </tr>
</table>

<br><br><center><img border="0" src="images/banner.gif" width="468" height="60"></center><br><br>
<?
include ("templates/footer.html");
?>
