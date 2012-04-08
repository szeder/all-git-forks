// Copyright (C) 2012, chaitanya nalla <nallachaitu@gmail.com>

/**
 * @fileOverview Generic JavaScript code (helper functions) using jquery
 * @license GPLv2 or later
 */


/* ============================================================ */
/* Manipulating links */

/**
 * used to check if link has 'js' query parameter already (at end),
 * and other reasons to not add 'js=1' param at the end of link
 * @constant
 */
var jsExceptionsRe = /[;?]js=[01](#.*)?$/;

/**
 * Add '?js=1' or ';js=1' to the end of a link in the document when it is clicked in future and 
 * that doesn't have 'js' query parameter set already.
 *
 * Links with 'js=1' lead to JavaScript version of given action, if it
 * exists (currently there is only 'blame_incremental' for 'blame')
 *
 * To be used as `window.onload` handler
 *
 * @globals jsExceptionsRe
 */
 function fixLinks(){
$(document).delegate("document.links","click",function(event){

		        if(!jsExceptionRe.test(event.targetElement.href())){
				$(event.targetElement).attr("href") = this.href.replace("/(#|$)/,
				(link.href.indexOf('?')=== -1 ? '?' : ';') + 'js=1$1');


			}



		});
}
/* end of javascript-detection.js */
