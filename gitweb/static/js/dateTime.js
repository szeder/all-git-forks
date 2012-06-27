/* DateTime library for gitweb
 * Author : Jaseem Abid <jaseemabid@gmail.com>
 */
(function (window) {
	"use strict";

	/* Necessary polyfils */
	Date.prototype.getDayName = function () {
		var days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
		return days[this.getDay()];
	};

	Date.prototype.getMonthName = function () {
		var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
		return months[this.getMonth()];
	};

	var dateTime = {
		parseTime : function (dateString) {
			/*!
			 * Takes in a UTC time by gitweb and returns a Date object
			 */
			return new Date(dateString);
		},
		getOffsetInMs : function (dateString) {
			/*! Gives ms offset from tz string
			 *	Expects '+1200' or '-0730'
			 *	Returns -19600 etc
			 */

			var tzRe = /^([+\-])([0-9][0-9])([0-9][0-9])$/,
				result = tzRe.exec(dateString),
				sign = result[1],
				hh = parseInt(result[2], 10),
				mm = parseInt(result[3], 10),
				off = (hh * 3600 + mm * 60) * 1000;
			// The flip in sign is to be compatible with Date.getTimezoneOffs)
			if (sign === '+') {
				off = -1 * off;
			}
			return off;
		},
		getOffsetString : function (timeInMs) {
			/*! Gives offset string from tz in MS
			 *	Expects -19600 etc
			 *	Returns '+1200' or '-0730'
			 */
			var sign = (timeInMs <= 0) ? '+' : '-',
				timeInS = Math.abs(timeInMs / 1000), // +v
				hh = parseInt(timeInS / (60 * 60), 10),
				mm = parseInt((timeInS - hh * (60 * 60)) / 60, 10);

			if (hh < 10) { // Padding to 2 digits
				hh = '0' + hh;
			}
			if (mm < 10) { // Padding to 2 digits
				mm = '0' + mm;
			}
			return [sign, hh, mm].join('');
		}
	}, DateView = function (dateString) {
		this.dateView = dateString;
		this.convertTo = function (target) {
			/*
			 * Converts to target timezone and updates this.dateView
			 * Params :
			 * target - target TimeZone as string like +0530 or -0700
			 */

			var targetOffsetInMs,
				targetTime = [],
				date,
				localOffsetInMs = new Date().getTimezoneOffset() * 60000,
				selfTime = new Date(this.dateView).getTime();

			if (target === "local") {
				// Convert "local" to -330 * 60 * 1000 etc
				targetOffsetInMs = (new Date()).getTimezoneOffset() * 60 * 1000;
				// Convert "local" to +0530 etc
				target = dateTime.getOffsetString(targetOffsetInMs);
			} else if (target === "utc") {
				targetOffsetInMs = 0;
				//target = dateTime.getOffsetString(targetOffsetInMs);
				target = "+0000";
			} else {
				targetOffsetInMs = dateTime.getOffsetInMs(target);
			}

			date = new Date(selfTime + localOffsetInMs - targetOffsetInMs);

			// Converting targetTime to custom format
			// Thu, 7 Apr 2005 22:13:13 +0000'

			targetTime = [];
			targetTime.push(date.getDayName() + ',');       // Thu,
			targetTime.push(date.getDate());                // 7
			targetTime.push(date.getMonthName());           // Apr
			targetTime.push(date.getUTCFullYear());         // 2005
			targetTime.push(date.toLocaleTimeString());     // 22:13:13
			targetTime.push(target);                        // +0000

			this.dateView = targetTime.join(' ');
			return this;
		};
	};

	window.gitweb = {
		DateView : DateView,
		dateTime : dateTime
	};

}(this));
