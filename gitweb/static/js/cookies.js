// Copyright (C) 2011, John 'Warthog9' Hawley <warthog9@eaglescrag.net>
//
//
// JavaScript cookie handling code for gitweb (git web interface).
// @license GPLv2 or later
//

function setCookieExp( name, value, expires ){
	var expDate = new Date( expires.toString() );
	expires = expDate.toUTCString;
	document.cookie = escape(name) +"="+ escape(value) +";"+ expDate.toUTCString() +";path=/";
}

function setCookie( name, value ){
	var txtCookie = name +"=\""+ value +"\";path=/";
	document.cookie = txtCookie;
}

function getCookie( name ){
	var allCookies = document.cookie.split(";");
	var value = "";

	for( var x = 0; x < allCookies.length; x++ ){
		var brokenCookie = allCookies[x].split("=",2);
		var hasName = new RegExp("^\\s*" + name + "\\s*$");
		if(
			hasName.test(brokenCookie[0])	// Check for the name of the cookie based on the regex
			&&
			brokenCookie.length == 2	// Just making sure there is something to actually return here
		){
			return unescape(brokenCookie[1]);
		}
	}
	return null;
}
