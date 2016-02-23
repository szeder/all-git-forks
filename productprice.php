<?
require("config.php"); // Configuration file
include("templates/header.html");

if (!isset($_SESSION['member_name']))
{
	header("Location: index.php");
}

if($tld)
{
echo "<center>Setting price for product <b>" . $tld . "</b> for member level <b>" . $sub_member_level . "</b><br><br>";

	$product_type = "dom" . $tld ;
	$params = array($login_info , $member_name , $product_type , 1);
	$result = $client->call("member_product_price", $params);
	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
		die();
	} 

	$params2 = array($login_info , $product_type , 1 , $sub_member_level);
	$result2 = $client->call("member_default_price", $params2);
	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result2['faultactor'] . "</font>";
		echo "<font color=red>" . $result2['faultstring'] . "</font>";
		die();
	} 

//echo "Your current cost for this product is " . $result['result']['details']; ?>
                        <FORM style="MARGIN: 0px" 
                        action=setproductprice.php method=post>
                        <TABLE cellSpacing=0 cellPadding=0 width="30%" 
border=0>
                          <TBODY>
                          <TR>
                            <TD width="38%"><input type="text" name="product_new_price" value="<? echo $result2['result']['details']; ?>"></TD>
                            <TD width="36%">
					<input type="hidden" name="sub_member_level" value="<? echo $sub_member_level; ?>">
					<input type="hidden" name="product_type" value="<? echo $product_type; ?>">
					<input type="hidden" name="s" value="<? echo $s; ?>">
					<INPUT type=submit value=Submit name=Submit> 

                          </TD></TR><tr><td colspan="2"></td></tr></TBODY></TABLE></FORM>
<br>
<? 
//echo "Your benefit is $ " .  $result2['result']['details'] - $result['result']['details']; 
} else {

	$params = array($login_info);
	$result = $client->call("member_level_list", $params);
	$err = $client->getError();
	if ($err) { 
		echo "<font color=red>" . $result['faultactor'] . "</font>";
		echo "<font color=red>" . $result['faultstring'] . "</font>";
		die();
	} 
?>
Please choose a product and a member level.<br><br>
                        <FORM style="MARGIN: 0px" 
                        action=productprice.php method=post>
                        <TABLE cellSpacing=0 cellPadding=0 width="30%" 
border=0>
                          <TBODY>
                          <TR>
                            <TD width="38%"><SELECT 
                              name=tld><option value="com_register">.com Registration
							<option value="net_register">.net Registration
							<option value="org_register">.org Registration
							<option value="info_register">.info Registration
							<option value="biz_register">.biz Registration
							<option value="us_register">.us Registration
							<option value="name_register">.name Registration
							<option value="in_register">.in Registration
							<option value="mobi_register">.mobi Registration
							<option value="bz_register">.bz Registration
							<option value="mn_register">.mn Registration
							<option value="cc_register">.cc Registration
							<option value="tv_register">.tv Registration
							
							<option value="com_transfer">.com Transfer
							<option value="net_transfer">.net Transfer
							<option value="org_transfer">.org Transfer
							<option value="info_transfer">.info Transfer
							<option value="biz_transfer">.biz Transfer
							<option value="us_transfer">.us Transfer
							<option value="name_transfer">.name Transfer
							<option value="in_transfer">.in Transfer
							<option value="mobi_transfer">.mobi Transfer
							<option value="bz_transfer">.bz Transfer
							<option value="mn_transfer">.mn Transfer
							<option value="cc_transfer">.cc Transfer
							<option value="tv_transfer">.tv Transfer

							<option value="com_renewal">.com Renewal
							<option value="net_renewal">.net Renewal
							<option value="org_renewal">.org Renewal
							<option value="info_renewal">.info Renewal
							<option value="biz_renewal">.biz Renewal
							<option value="us_renewal">.us Renewal
							<option value="name_renewal">.name Renewal
							<option value="in_renewal">.in Renewal
							<option value="mobi_renewal">.mobi Renewal
							<option value="bz_renewal">.bz Renewal
							<option value="mn_renewal">.mn Renewal
							<option value="cc_renewal">.cc Renewal
							<option value="tv_renewal">.tv Renewal
							</SELECT>
</td><td width="50%">
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
					<input type="hidden" name="s" value="<? echo $s; ?>">
					<INPUT type=submit value=Submit name=Submit> 

                          </TD></TR></TBODY></TABLE></FORM>
<?
}
include("templates/footer.html");
?>