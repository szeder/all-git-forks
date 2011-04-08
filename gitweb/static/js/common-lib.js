// Copyright (C) 2011, John 'Warthog9' Hawley <warthog9@eaglescrag.net>
//
//
// JavaScript date handling code for gitweb (git web interface).
// @license GPLv2 or later
//
//
function findElementsByClassName( className ) {
	if( document.getElementsByClassName == undefined) {
		var hasClassName = new RegExp("(?:^|\\s)" + className + "(?:$|\\s)");
		var allElements = document.getElementsByTagName("*");
		var foundElements = [];

		var element = null;
		for (var x = 0; (element = allElements[x]) != null; x++) {
			var curClass = element.className;
			if(
				curClass				// If we've actually got something
				&&
				curClass.indexOf(className) != -1	// And it has a valid index, I.E. not -1
				&&
				hasClassName.test(curClass)		// and the regex passes
			) {
				foundElements.push(element);		// push it onto the results stack
			}
		}

		return foundElements;
	}else{
		return document.getElementsByClassName( className );
	}
}
