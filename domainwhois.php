<?
require("config.php"); // Configuration file
include ("templates/header.html");

$server = "whois.crsnic.net";
#Determine which WHOIS server to use for the supplied TLD
if((eregi("\.com\$|\.net\$|\.edu\$", $host)) || (eregi("\.com\$|\.net\$|\.edu\$", $nhost)))
  $server = "whois.crsnic.net";
else if((eregi("\.info\$", $host)) || (eregi("\.info\$", $nhost)))
  $server = "whois.afilias.net";
else if((eregi("\.org\$", $host)) || (eregi("\.org\$", $nhost)))
  $server = "whois.corenic.net";
else if((eregi("\.name\$", $host)) || (eregi("\.name\$", $nhost)))
  $server = "whois.nic.name";
else if((eregi("\.biz\$", $host)) || (eregi("\.biz\$", $nhost)))
  $server = "whois.nic.biz";
else if((eregi("\.us\$", $host)) || (eregi("\.us\$", $nhost)))
  $server = "whois.nic.us";
else if((eregi("\.cc\$", $host)) || (eregi("\.cc\$", $nhost)))
  $server = "whois.enicregistrar.com";
else if((eregi("\.ws\$", $host)) || (eregi("\.ws\$", $nhost)))
  $server = "whois.nic.ws";
else if((eregi("\.it\$", $host)) || (eregi("\.it\$", $nhost)))
  $server = "whois.nic.it";
else{
  $msg .= "Invalid domain name";
echo $msg;
  return;
}
//echo "Connecting to $server...<br><br>";
if (! $sock = fsockopen($server, 43, $num, $error, 10)){
  unset($sock);
  $msg .= "Timed-out while connecting to $server on port 43";
}
else{
  fputs($sock, "$host\n");
  while (!feof($sock))
    $buffer .= fgets($sock, 10240); 
}
 fclose($sock);
 if(! eregi("Whois Server:", $buffer)){
   if(eregi("no match", $buffer))
     echo "No match for $host<br>";
   else
     echo "Multiple matches for $host:<br>";
 }
 else{
   $buffer = split("\n", $buffer);
   for ($i=0; $i<sizeof($buffer); $i++){
     if (eregi("Whois Server:", $buffer[$i]))
       $buffer = $buffer[$i];
   }
   $nextServer = substr($buffer, 17, (strlen($buffer)-17));
   $nextServer = str_replace("1:Whois Server:", "", trim(rtrim($nextServer)));
   $buffer = "";
//   echo "Refered to whois server: $nextServer...<br><br>";
   if(! $sock = fsockopen($nextServer, 43, $num, $error, 10)){
     unset($sock);
     $msg .= "Timed-out while connecting to $nextServer (port 43)";
   }
   else{
     fputs($sock, "$host\n");
     while (!feof($sock))
       $buffer .= fgets($sock, 10240);
     fclose($sock);
   }
}
$msg .= nl2br($buffer);

echo "<p align=left>" . $msg . "</p>";

include ("templates/footer.html");
?>