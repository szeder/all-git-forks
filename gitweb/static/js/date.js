// Copyright (C) 2011, John 'Warthog9' Hawley <warthog9@eaglescrag.net>
//
//
// JavaScript date handling code for gitweb (git web interface).
// @license GPLv2 or later
//

function onloadTZSetup(){
	addChangeTZ();
	tzChangeNS( tzDefault );
	checkTZCookie();
}

function addChangeTZ() {
		var txtClassesFound = "";
		var classesFound = findElementsByClassName( "dtcommit" );
		txtClassesFound += "Length: "+ classesFound.length +"<br>\n";
		for ( x = 0; x < classesFound.length; x++){
			curElement = classesFound[x];
			txtClassesFound += "<br>\n"+ x +" - "+ curElement.nodeName  +" - "+ curElement.title +" - "+ curElement.innerHTML +"<br>\n";
			var strExtra = " <span onclick=\"clickDate(event.target);\" title=\"+\">+</span>"
			curElement.innerHTML = curElement.innerHTML + strExtra;
		}
}

function checkTZCookie(){
	var preSetTZ = getCookie( getwebCookieTZOffset );
	if(
		preSetTZ != null
		&&
		preSetTZ.length != 0
	){
		tzChange( preSetTZ );
	}
}

function formatTZ( tzOffset ) {
		var posNeg = "+";
		if( tzOffset < 0 ){
			posNeg = "-";
		}
		tzOffset = Math.sqrt( Math.pow( tzOffset, 2 ) );
		if( tzOffset < 100 ){
			tzOffset = tzOffset * 100;
		}
		for( y = tzOffset.toString().length + 1; y <= 4; y++ ){
			tzOffset = "0"+ tzOffset;
		}
		return posNeg + tzOffset;
}

function dateOutput( objDate ) {
	return dateOutputTZ( objDate, "0" );
}

function dateOutputTZ( objDate, tzOffset ) {
	var strDate = "";
	var daysOfWeek = [ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" ];
	var monthsOfYr = [ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" ];

	if( tzOffset == "utc" ){
		tzOffset = 0;
	}else if( tzOffset == "local" ){
		var tempDate = new Date();
		tzOffset = tempDate.getTimezoneOffset() * -1 / 60 * 100;
		tzOffset = formatTZ( tzOffset );
	}

	var msPerHr = 1000 * 60 * 60;	// 1000ms/sec * 60sec/min * 60sec/hr
	var toDateTime = new Date();
	toDateTime.setTime( objDate.getTime() + ( tzOffset / 100 * msPerHr ) );

	// Current Date Formatting:
	// Fri, 19 Dec 2008 11:35:33 +0000
	strDate += daysOfWeek[ toDateTime.getUTCDay() ] +", ";
	strDate += toDateTime.getUTCDate() +" ";
	strDate += monthsOfYr[ toDateTime.getUTCMonth() ] +" ";
	strDate += toDateTime.getUTCFullYear() +" ";
	strDate += toDateTime.getUTCHours() +":";
	strDate += toDateTime.getUTCMinutes() +":";
	strDate += toDateTime.getUTCSeconds() +" ";

	strDate += tzOffset;

	return strDate;
}

function tzChange( tzOffset ){
	return tzChangeSNS( tzOffset, true );
}

function tzChangeNS( tzOffset ){
	return tzChangeSNS( tzOffset, false );
}

function tzChangeSNS( tzOffset, set ){
	var txtClassesFound = "";
	var classesFound = findElementsByClassName( "dtcommit" );
	for ( x = 0; x < classesFound.length; x++){
		curElement = classesFound[x];
		var origDateTime = new Date( curElement.title );
		curElement.innerHTML = dateOutputTZ(origDateTime, tzOffset);
	}
	var tzExpDate = new Date();
	tzExpDate.setDate( tzExpDate.getDate() + 180 );
	if( set == true ){
		setCookieExp( getwebCookieTZOffset, tzOffset, tzExpDate.toUTCString() );
	}
	addChangeTZ();
}

function clickDate( clkEvent ) {
	if( clkEvent.title == "+" ){
		clkEvent.title="-";

		var preSetTZ = getCookie( getwebCookieTZOffset );

		var arrSelected = new Array();
		var offsetArr = 14;
		arrSelected[0] = " ";
		arrSelected[1] = " ";
		if( preSetTZ == "utc" ) {
			arrSelected[0] = " selected=\"selected\" ";
		} else if( preSetTZ == "local" ){
			arrSelected[1] = " selected=\"selected\" ";
		}
		for( x = -12; x <= 12; x++){
			arrSelected[x + offsetArr] = "";
			if( ( x * 100 ) == preSetTZ ){
				arrSelected[x + offsetArr] = " selected=\"selected\" ";
			}
		}
		var txtTzSelect = " \
<span style=\"width: 10%;background-color: grey;\">- \
<table border=\"1\">\
	<tr>\
		<td>\
			Time Zone:\
		</td>\
		<td>\
			<select name=\"tzoffset\" onchange=\"tzChange(this.value);\">\
				<option "+ arrSelected[0] +" value=\"utc\">UTC/GMT</option>\
				<option "+ arrSelected[1] +" value=\"local\">Local (per browser)</option>";
		for( x = -12; x <= 12; x++){
			var tzOffset = formatTZ( x );
			txtTzSelect +="					<option "+ arrSelected[x + offsetArr] +"value=\""+ tzOffset +"\">"+ tzOffset +"</option>";
		}
		txtTzSelect += "\
			</select>\
		</td>\
	</tr>\
</table>\
</span>";
		clkEvent.innerHTML = txtTzSelect;
	}else{
		clkEvent.parentNode.title="+";
		clkEvent.parentNode.innerHTML = "+";
	}
}
