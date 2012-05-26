describe("Datetime library", function(){

	describe ("timezoneOffset" , function(){

		it("should work for zero offset", function(){
			expect(timezoneOffset("+0000")).toEqual(0);
			expect(timezoneOffset("-0000")).toEqual(0);
		});

		it("should work for positive offset", function(){
			expect(timezoneOffset("+0530")).toEqual(19800);
		});

		it("should work for negative offset", function(){
			expect(timezoneOffset("-2400")).toEqual(-86400);
		});

		xit("should work for offsets more than 24hrs", function(){
			// Hold this one for now
			// Expect 25 hour to be equal to 1 hour
			expect(timezoneOffset("-2500")).toEqual(-3600);
		});

	});

})
