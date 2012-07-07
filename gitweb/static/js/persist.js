/*!
 * Persistant storage for timezone preference
 * Author : Jaseem Abid <jaseemabid@gmail.com>
 */

(function () {
	"use strict";

	/* gitweb.tZPreference need to be set from the perl script.
	 * Expecting TZ strings of the format +05:30
	 */
	window.gitweb.persist = {
		getData : function () {
			/* Return stored data
			 * TODO :
			 * Use webstorage by default & fall back to cookies
			 * */
			/* Return localStorage value or default */
			if (localStorage) {
				gitweb.tZPreference = localStorage.gitweb_tZPreference;
			}
			return gitweb.tZPreference || "local";
		},
		setData : function (val) {
			/* Sava data persistently
			 * TODO :
			 * Use webstorage by default & fall back to cookies
			 * */
			gitweb.tZPreference = val;
			if (localStorage) {
				localStorage.gitweb_tZPreference = val;
			}
		}
	};
}());
