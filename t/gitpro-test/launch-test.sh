#!/bin/bash

./before-test.sh

# If number of arguments less then 1; print usage and exit
if [ $# -lt 1 ]; then
    printf "Usage: $0 <application> <test_file_name>\n"
    exit 1
fi

bin="$1"           # The application (from command arg)
file_base="$2"	   # The file name to test (.in and .out had to have same name)
diff="diff -iad"   # Diff command, or what ever


# Padd file_base with suffixes
file_in="test_input/$file_base.in"             # The in file
file_out_val="test_output/$file_base.out"       # The out file to check against
file_out_tst="test_output/$file_base.out.tst"   # The outfile from test application

# Validate infile exists (do the same for out validate file)
if [ ! -f "$file_in" ]; then
printf "File %s is missing\n" "$file_in"
exit;
fi
if [ ! -f "$file_out_val" ]; then
printf "Validation file %s is missing\n" "$file_out_val"
exit;    
fi

# Run application, redirect in file to app, and output to out file
cd "../../"
test_path="t/gitpro-test"
eval "./$bin < $test_path/$file_in > $test_path/$file_out_tst"
cd $test_path

# Execute diff
$diff "$file_out_tst" "$file_out_val"


# Check exit code from previous command (ie diff)
# We need to add this to a variable else we can't print it
# as it will be changed by the if [
# Iff not 0 then the files differ (at least with diff)
e_code=$?
if [ $e_code != 0 ]; then
    printf "%s FAILED\n" "$file_base"
else
    printf "%s ok\n" "$file_base"
fi

./after-test.sh

# Clean exit with status 0
exit 0
