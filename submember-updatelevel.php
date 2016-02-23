<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if (strlen($sub_member_level) != 0)
{
$result = $client->call("member_update_level", array($login_info , $sub_member_name , $sub_member_level));
$err = $client->getError();
if ($err) { 
	echo "<font color=red>" . $result['faultactor'] . "</font>";
	echo "<font color=red>" . $result['faultstring'] . "</font>";
} else {
	echo $result['result']['details'];
}

} else {
if (!$sub_member_name){
	echo "Invalid member name";
	die();
}
echo $sub_member_name; 

	$params = array($login_info);
	$result = $client->call("member_level_list", $params);
	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
		die();
	} 

?>

                        <FORM style="MARGIN: 0px" 
                        action=submember-updatelevel.php method=post>
                        <TABLE cellSpacing=0 cellPadding=0 width="30%" 
border=0>
                          <TBODY>
                          <TR>
<td width="50%">
<SELECT 
                              name=sub_member_level>
<?
	for($i=0;$i<=$result['result']['details'];$i++)
	{
?>
<option value="<? echo $i; ?>">Member level <? echo $i; ?></option>
<?
	}
?>
							</SELECT>

 </TD>
                            <TD width="36%">
					<input type="hidden" name="sub_member_name" value="<? echo $sub_member_name; ?>">
					<input type="hidden" name="s" value="<? echo $s; ?>">
					<INPUT type=submit value=Submit name=Submit> 

                          </TD></TR></TBODY></TABLE></FORM>
<?
}
include("templates/footer.html");
?>