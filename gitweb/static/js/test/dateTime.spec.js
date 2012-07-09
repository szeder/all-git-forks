describe("Datetime library functions", function () {
	"use strict";
	var tString = "Thu, 7 Apr 2005 15:13:13 -0700",
		tRe = /\w{3},\s\d{1,2}\s\w{3}\s\d{4}\s\d{1,2}:\d{1,2}:\d{1,2}\s[+|\-]\d{4}/,
		tzRe = /^([+\-])([0-9][0-9])([0-9][0-9])$/;

	describe("DateView constructor", function () {
		var DateView = gitweb.DateView;
		it("should set the argument unmodified", function () {
			var dv = new DateView(tString);
			dv.dateView.should.equal(tString);
		});

		describe("Convert time strings", function () {

			var tString        = "Thu, 7 Apr 2005 15:13:13 -0700",
				tStringInP0530 = "Fri, 8 Apr 2005 03:43:13 +0530",
				tStringInP1200 = "Fri, 8 Apr 2005 10:13:13 +1200",
				tStringInM0500 = "Thu, 7 Apr 2005 17:13:13 -0500",
				tStringInM1100 = "Thu, 7 Apr 2005 11:13:13 -1100",
				tStringInUTC   = "Thu, 7 Apr 2005 22:13:13 +0000";

			it("to UTC", function () {
				var dv = new DateView(tString);
				dv.convertTo('+0000').dateView.should.equal(tStringInUTC);
				dv.convertTo('utc').dateView.should.equal(tStringInUTC);
			});

			it("to +ve TZ", function () {
				var dv = new DateView(tString);
				dv.convertTo('+0530').dateView.should.equal(tStringInP0530);
				dv.convertTo('+1200').dateView.should.equal(tStringInP1200);
			});

			it("to -ve TZ", function () {
				var dv = new DateView(tString);
				dv.convertTo('-0500').dateView.should.equal(tStringInM0500);
				dv.convertTo('-1100').dateView.should.equal(tStringInM1100);
			});

			it("with std strings", function () {
				var dv = new DateView(tString);
				dv.convertTo('local').dateView.should.equal(tStringInP0530);
				dv.convertTo('utc').dateView.should.equal(tStringInUTC);
			});
		});
	});

	describe("dateTime support functions", function () {

		describe("getOffsetInMs", function () {
			var getOffsetInMs = gitweb.dateTime.getOffsetInMs;

			it("should find negative offsets", function () {
				getOffsetInMs("-0700").should.equal(7 * 3600 * 1000);
				getOffsetInMs("-1234").should.equal((12 * 3600 + 34 * 60) * 1000);
			});

			it("should find positive offsets", function () {
				getOffsetInMs("+0700").should.equal(-7 * 3600 * 1000);
			});

			it("should work for zero offsets", function () {
				getOffsetInMs("-0000").should.equal(0);
				getOffsetInMs("+0000").should.equal(0);
			});
		});

		describe("getOffsetString", function () {

			var getOffsetString = gitweb.dateTime.getOffsetString;

			it("should find negative offsets", function () {
				getOffsetString(7 * 3600 * 1000).should.equal('-0700');
			});

			it("should find positive offsets", function () {
				getOffsetString(-7 * 3600 * 1000).should.equal('+0700');
				getOffsetString(-5.5 * 3600 * 1000).should.equal('+0530');

			});

			it("should work for zero offsets", function () {
				getOffsetString(0).should.equal('+0000');
			});


			it("should work for random offsets", function () {
				var rand = parseInt(Math.random() * 100000000, 10);
				getOffsetString(rand).should.match(tzRe);
			});
		});
	});
});
