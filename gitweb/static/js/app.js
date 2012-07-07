/*!
 * Code to be excecuted on page load
 * Author : Jaseem Abid <jaseemabid@gmail.com>
 */

(function ($, gitweb) {
	"use strict";

	$(document).ready(function () {
		var popup,
			select,
			oldTime,
			tz,
			op,
			persist = gitweb.persist.getData(),
			timeZones =  gitweb.timeZones;

		/* Create TZ selector from timeZones */
		for (tz in timeZones) {
			if (timeZones.hasOwnProperty(tz)) {
				if (tz === persist) {
					/* <option selected="1" value="utc">UTC/GMT</option> */
					op = ['<option selected="1" value="', tz, '">', timeZones[tz], '</option>'];
				} else {
					/* <option value="utc">UTC/GMT</option> */
					op = ['<option value="', tz, '">', timeZones[tz], '</option>'];
				}
				select += op.join('');
			}
		}

		select = ['<select>', select, '</select>'].join('');

		/* Create the popup */
		popup = $("<div>")
			.addClass("popup")
			.append(
				$("<div>")
					.addClass("close-button")
					.attr("title", "Click to close")
					.html("X")
			)
			.append('<span>Select timezone:</span><br clear="all" />')
			.append(select);

		/* Insert the popup on click */
		$('span.datetime').click(function () {
			$(this)
				.css({'position': 'relative',
					  'display': 'inline-block'});
			oldTime = $(this).text();
			$(this).append(popup);
		});

		/* Close button on popup */
		$("body").on("click", "div.close-button", function () {
			$(this).parent().remove();
		});

		/* Update all timestamps to preferred TZ */
		$('span.datetime').each(function (index) {
			// console.log(index,this);
			oldTime = $(this).text();
			var newTime = new gitweb.DateView(oldTime).convertTo(persist).dateView;
			// Update with new time
			$(this).text(newTime);
		});

		/* Trigger event on timezone selection */
		$("body").on("change", "select", function () {
			var span = $(this).parent().parent('span.datetime'),
				newTz = $(this).val(),
				newTime = new gitweb.DateView(oldTime).convertTo(newTz).dateView;
			// Remove the popup after val selection
			$(this).parent().remove();
			// Update with new time
			span.text(newTime);
			// Update localStorage
			gitweb.persist.setData(newTz);
		});
	});
}(jQuery, window.gitweb));
