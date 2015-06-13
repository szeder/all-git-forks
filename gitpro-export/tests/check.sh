#!/bin/bash

# If number of arguments less then 1; print usage and exit
if [ $# -lt 2 ]; then
    printf "Usage: $0 <test-name> <result-file> <expected-file>\n"
    exit 1
fi

testname=$1
result=$2
expected=$3

diff="diff -iad" 	# Diff command, or what ever

# Validate files exists 
if [ ! -f "$result" ]; then
printf "File %s is missing\n" "$result"
exit;
fi
if [ ! -f "$expected" ]; then
printf "File %s is missing\n" "$expected"
exit;    
fi

# Execute diff
$diff "$result" "$expected"


# Check exit code from previous command (ie diff)
# We need to add this to a variable else we can't print it
# as it will be changed by the if [
# Iff not 0 then the files differ (at least with diff)
e_code=$?
if [ $e_code != 0 ]; then
    printf "%s FAILED\n" "$testname"
else
    printf "%s ok\n" "$testname"
fi

rm "$result"
rm "$expected"

# Clean exit with status 0
exit 0
