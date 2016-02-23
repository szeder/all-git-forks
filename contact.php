<?
require("config.php"); // Configuration file
include ("templates/header.html");
if (strlen($_POST['yourname'])<1 || strlen($_POST['youremail'])<1 || strlen($_POST['subject'])<1 || strlen($_POST['message'])<1)
{
include ("templates/contact.html");
}
	else {
$headers .= "From: $yourname <$youremail>\n";
$headers .= "Content-Type: text/html; charset=iso-8859-1\n";
 
$Send = mail($parent_contact_email, $subject,$message,$headers); 

	if ($Send) { 
	echo "<font face=tahoma>Your message has been sent, We will reply you soon.</font>"; 
	} else { 
	echo "<font face=tahoma color=red>An error has occured, please try again.</font>"; 
	} 
}
include ("templates/footer.html");
?>
