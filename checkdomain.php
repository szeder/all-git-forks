<?php

if($domain)
{
	$domain_details_hash = array();
	foreach($_POST[tld_list] as $tld){
		$domain_name = $domain . "." . $tld;
		$domain_details_hash[] = Array("domain_name" => $domain_name);
	}
		$params = array($parent_login_info , $domain_details_hash);
		$result = $client->call("domain_check", $params);
		$err = $client->getError();
		if ($err) { 
			echo "<font color=red>" . $result['faultactor'] . "</font>";
			echo "<font color=red>" . $result['faultstring'] . "</font>";
		} else {
			for($i=0;$i<=$result['result']['number_of_records']-1;$i++)
			{
				echo "<b>" . $result['result']['details'][$i]['domain_name'] . "</b> is " . $result['result']['details'][$i]['details'];
				if(strstr($result['result']['details'][$i]['details'], "Not"))
				{  
					echo "[<a href=domainwhois.php?host=" . $result['result']['details'][$i]['domain_name'] . ">Whois</a>][<a href=transferdomain.php?domain_name=" . $result['result']['details'][$i]['domain_name'] . "&s=" . $s .">Transfer</a>]<br>";
				} else {
					echo "[<a href=registerdomain.php?domain_name=" . $result['result']['details'][$i]['domain_name'] . "&s=" . $s .">Register</a>]<br>";
				}
			}
		}
} else {
include ("templates/checkdomain.html");
}
?>
