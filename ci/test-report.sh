#!/bin/sh
#
# Print test results
#
for TEST_EXIT in t/test-results/*.exit
do
	if test "$(cat "$TEST_EXIT")" != "0"
	then
		TEST="${TEST_EXIT%.exit}"
		TEST_OUT="${TEST}.out"
		echo "------------------------------------------------------------------------"
		echo "  $(tput setaf 1)${TEST} Output$(tput sgr0)"
		echo "------------------------------------------------------------------------"
		cat "$TEST_OUT"
		echo ""
		echo ""
	fi
done

