#!/bin/bash

source constants.sh

###########################
# 	TASK Delete TESTS
###########################
echo "testing: git task -d"


# TEST 1 --- delete001 --- Delete all task (no filters) (multiple tasks exists)
cat > "$input/delete001.in" << \EOF
EOF
cat > "$output/delete001.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 4 ...
- Task 4 prepared to be removed
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
- Removing asignations and asociations to task 7 ...
- Task 7 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d' 'delete001' 'no-asignations'

# TEST 2 --- delete002 --- Delete one task (id filter) (task exists)
cat > "$input/delete002.in" << \EOF
EOF
cat > "$output/delete002.out" << \EOF
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --id 3' 'delete002' 'no-asignations'

# TEST 3 --- delete003 --- Delete task by name (had to match exactly) (one task finded)
cat > "$input/delete003.in" << \EOF
EOF
cat > "$output/delete003.out" << \EOF
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -n "task 5"' 'delete003' 'no-asignations'

# TEST 4 --- delete004 --- Delete task by state (multiple task matches) (minus)
cat > "$input/delete004.in" << \EOF
EOF
cat > "$output/delete004.out" << \EOF
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s "in progress"' 'delete004' 'no-asignations'

# TEST 5 --- delete005 --- Delete task by state (multiple task matches) (mayus)
cat > "$input/delete005.in" << \EOF
EOF
cat > "$output/delete005.out" << \EOF
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s "IN PROGRESS"' 'delete005' 'no-asignations'

# TEST 6 --- delete006 --- Delete task by state (multiple task matches) (mayus & minus)
cat > "$input/delete006.in" << \EOF
EOF
cat > "$output/delete006.out" << \EOF
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s "iN proGrEss"' 'delete006' 'no-asignations'

# TEST 7 --- delete007 --- Delete by id (invalid id)
cat > "$input/delete007.in" << \EOF
EOF
cat > "$output/delete007.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --id hola' 'delete007'

# TEST 8 --- delete008 --- Delete by id (** All selected task removed successfully)
cat > "$input/delete008.in" << \EOF
EOF
cat > "$output/delete008.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --id 20' 'delete008' 'no-asignations'

# TEST 9 --- delete009 --- Delete by name (only part of name matches)
cat > "$input/delete009.in" << \EOF
EOF
cat > "$output/delete009.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -n inexistent' 'delete009' 'no-asignations'

# TEST 10 --- delete010 --- Delete task by name (part of name matching only with one task)
cat > "$input/delete010.in" << \EOF
EOF
cat > "$output/delete010.out" << \EOF
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -n 5' 'delete010' 'no-asignations'

# TEST 11 --- delete011 --- Delete task by name (part of name matching with multiple task names)
cat > "$input/delete011.in" << \EOF
EOF
cat > "$output/delete011.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 4 ...
- Task 4 prepared to be removed
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -n task' 'delete011' 'no-asignations'

# TEST 12 --- delete012 --- Delete task by state (only one matching) (minus)
cat > "$input/delete012.in" << \EOF
EOF
cat > "$output/delete012.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s new' 'delete012' 'no-asignations'

# TEST 13 --- delete013 --- Delete task by state (only one matching) (mayus)
cat > "$input/delete013.in" << \EOF
EOF
cat > "$output/delete013.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s NEW' 'delete013' 'no-asignations'

# TEST 14 --- delete014 --- Delete task by state (only one matching) (mixed mayus and minus)
cat > "$input/delete014.in" << \EOF
EOF
cat > "$output/delete014.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s New' 'delete014' 'no-asignations'

# TEST 15 --- delete015 --- Delete task by state (invalid / inexistent state)
cat > "$input/delete015.in" << \EOF
EOF
cat > "$output/delete015.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d -s inexistent' 'delete015'

# TEST 16 --- delete016 --- Delete task by estimated init date (invalid date letters)
cat > "$input/delete016.in" << \EOF
EOF
cat > "$output/delete016.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start date' 'delete016'

# TEST 17 --- delete017 --- Delete task by estimated init date (invalid day in date)
cat > "$input/delete017.in" << \EOF
EOF
cat > "$output/delete017.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 50/12/2014' 'delete017'

# TEST 18 --- delete018 --- Delete task by estimated init date (invalid month in date)
cat > "$input/delete018.in" << \EOF
EOF
cat > "$output/delete018.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 15/16/2014' 'delete018'

# TEST 19 --- delete019 --- Delete task by estimated init date (invalid year in date)
cat > "$input/delete019.in" << \EOF
EOF
cat > "$output/delete019.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 15/05/23' 'delete019'

# TEST 20 --- delete020 --- Delete task by estimated init date (invalid day in date) (negative)
cat > "$input/delete020.in" << \EOF
EOF
cat > "$output/delete020.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start -12/06/2014' 'delete020'

# TEST 21 --- delete021 --- Delete task by estimated init date (invalid day in date) (zero)
cat > "$input/delete021.in" << \EOF
EOF
cat > "$output/delete021.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 00/06/2014' 'delete021'

# TEST 22 --- delete022 --- Delete task by estimated  init date (invalid month in date) (zero)
cat > "$input/delete022.in" << \EOF
EOF
cat > "$output/delete022.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 15/00/2014' 'delete022'

# TEST 23 --- delete023 --- Delete task by estimated init date (invalid month in date) (negative)
cat > "$input/delete023.in" << \EOF
EOF
cat > "$output/delete023.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 15/-11/2014' 'delete023'

# TEST 24 --- delete024 --- Delete task by estimated init date (invalid year in date) (over four numbers)
cat > "$input/delete024.in" << \EOF
EOF
cat > "$output/delete024.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 15/05/20146' 'delete024'

# TEST 25 --- delete025 --- Delete task by estimated init date (invalid year in date) (negative)
cat > "$input/delete025.in" << \EOF
EOF
cat > "$output/delete025.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_start 15/11/-2014' 'delete025'

# TEST 26 --- delete026 --- Delete task by estimated final date (invalid date letters)
cat > "$input/delete026.in" << \EOF
EOF
cat > "$output/delete026.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end date' 'delete026'

# TEST 27 --- delete027 --- Delete task by estimated final date (invalid day in date)
cat > "$input/delete027.in" << \EOF
EOF
cat > "$output/delete027.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 50/12/2014' 'delete027'

# TEST 28 --- delete028 --- Delete task by estimated final date (invalid month in date)
cat > "$input/delete028.in" << \EOF
EOF
cat > "$output/delete028.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 15/16/2014' 'delete028'

# TEST 29 --- delete029 --- Delete task by estimated final date (invalid year in date)
cat > "$input/delete029.in" << \EOF
EOF
cat > "$output/delete029.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 15/05/23' 'delete029'

# TEST 30 --- delete030 --- Delete task by estimated final date (invalid day in date) (negative)
cat > "$input/delete030.in" << \EOF
EOF
cat > "$output/delete030.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end -12/10/2014' 'delete030'

# TEST 31 --- delete031 --- Delete task by estimated final date (invalid day in date) (zero)
cat > "$input/delete031.in" << \EOF
EOF
cat > "$output/delete031.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 00/05/2014' 'delete031'

# TEST 32 --- delete032 --- Delete task by estimated final date (invalid month in date) (zero)
cat > "$input/delete032.in" << \EOF
EOF
cat > "$output/delete032.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 15/00/2014' 'delete032'

# TEST 33 --- delete033 --- Delete task by estimated final date (invalid month in date) (negative)
cat > "$input/delete033.in" << \EOF
EOF
cat > "$output/delete033.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 15/-11/2014' 'delete033'

# TEST 34 --- delete034 --- Delete task by estimated final date (invalid year in date) (over four numbers)
cat > "$input/delete034.in" << \EOF
EOF
cat > "$output/delete034.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 15/05/20146' 'delete034'

# TEST 35 --- delete035 --- Delete task by estimated final date (invalid year in date) (negative)
cat > "$input/delete035.in" << \EOF
EOF
cat > "$output/delete035.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_end 15/11/-2014' 'delete035'

# TEST 36 --- delete036 --- Delete task by real init date (invalid date letters)
cat > "$input/delete036.in" << \EOF
EOF
cat > "$output/delete036.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start date' 'delete036'

# TEST 37 --- delete037 --- Delete task by real init date (invalid day in date)
cat > "$input/delete037.in" << \EOF
EOF
cat > "$output/delete037.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 50/12/2014' 'delete037'

# TEST 38 --- delete038 --- Delete task by real init date (invalid month in date)
cat > "$input/delete038.in" << \EOF
EOF
cat > "$output/delete038.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 15/16/2014' 'delete038'

# TEST 39 --- delete039 --- Delete task by real init date (invalid year in date)
cat > "$input/delete039.in" << \EOF
EOF
cat > "$output/delete039.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 15/05/23' 'delete039'

# TEST 40 --- delete040 --- Delete task by real init date (invalid day in date) (negative)
cat > "$input/delete040.in" << \EOF
EOF
cat > "$output/delete040.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start -12/10/2014' 'delete040'

# TEST 41 --- delete041 --- Delete task by real init date (invalid day in date) (zero)
cat > "$input/delete041.in" << \EOF
EOF
cat > "$output/delete041.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 00/11/2014' 'delete041'

# TEST 42 --- delete042 --- Delete task by real init date (invalid month in date) (zero)
cat > "$input/delete042.in" << \EOF
EOF
cat > "$output/delete042.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 15/00/2014' 'delete042'

# TEST 43 --- delete043 --- Delete task by real init date (invalid month in date) (negative)
cat > "$input/delete043.in" << \EOF
EOF
cat > "$output/delete043.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 15/-11/2014' 'delete043'

# TEST 44 --- delete044 --- Delete task by real init date (invalid year in date) (over four numbers)
cat > "$input/delete044.in" << \EOF
EOF
cat > "$output/delete044.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 15/05/20146' 'delete044'

# TEST 45 --- delete045 --- Delete task by real init date (invalid year in date) (negative)
cat > "$input/delete045.in" << \EOF
EOF
cat > "$output/delete045.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --start 15/11/-2014' 'delete045'

# TEST 46 --- delete046 --- Delete task by real final date (invalid date letters)
cat > "$input/delete046.in" << \EOF
EOF
cat > "$output/delete046.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end date' 'delete046'

# TEST 47 --- delete047 --- Delete task by real final date (invalid day in date)
cat > "$input/delete047.in" << \EOF
EOF
cat > "$output/delete047.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 50/12/2014' 'delete047'

# TEST 48 --- delete048 --- Delete task by real final date (invalid month in date)
cat > "$input/delete048.in" << \EOF
EOF
cat > "$output/delete048.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 15/16/2014' 'delete048'

# TEST 49 --- delete049 --- Delete task by real final date (invalid year in date)
cat > "$input/delete049.in" << \EOF
EOF
cat > "$output/delete049.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 15/05/23' 'delete049'

# TEST 50 --- delete050 --- Delete task by real final date (invalid day in date) (negative)
cat > "$input/delete050.in" << \EOF
EOF
cat > "$output/delete050.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end -12/10/2014' 'delete050'

# TEST 51 --- delete051 --- Delete task by real init date (invalid day in date) (zero)
cat > "$input/delete051.in" << \EOF
EOF
cat > "$output/delete051.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 00/11/2014' 'delete051'

# TEST 52 --- delete052 --- Delete task by real final date (invalid month in date) (zero)
cat > "$input/delete052.in" << \EOF
EOF
cat > "$output/delete052.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 15/00/2014' 'delete052'

# TEST 53 --- delete053 --- Delete task by real final date (invalid month in date) (negative)
cat > "$input/delete053.in" << \EOF
EOF
cat > "$output/delete053.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 15/-11/2014' 'delete053'

# TEST 54 --- delete054 --- Delete task by real final date (invalid year in date) (over four numbers)
cat > "$input/delete054.in" << \EOF
EOF
cat > "$output/delete054.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 15/05/20146' 'delete054'

# TEST 55 --- delete055 --- Delete task by real final date (invalid year in date) (negative)
cat > "$input/delete055.in" << \EOF
EOF
cat > "$output/delete055.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --end 15/11/-2014' 'delete055'

# TEST 56 --- delete056 --- Delete by estimated init date (valid) (one task matching)
cat > "$input/delete056.in" << \EOF
EOF
cat > "$output/delete056.out" << \EOF
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --est_start 30/12/2014' 'delete056' 'no-asignations'

# TEST 57 --- delete057 --- Delete by estimated final date (valid) (one task matching)
cat > "$input/delete057.in" << \EOF
EOF
cat > "$output/delete057.out" << \EOF
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --est_end 24/12/2014' 'delete057' 'no-asignations'

# TEST 58 --- delete058 --- Delete by real init date (valid) (one task matching)
cat > "$input/delete058.in" << \EOF
EOF
cat > "$output/delete058.out" << \EOF
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --start 21/12/2014' 'delete058' 'no-asignations'

# TEST 59 --- delete059 --- Delete by real final date (valid) (one task matching)
cat > "$input/delete059.in" << \EOF
EOF
cat > "$output/delete059.out" << \EOF
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --end 28/12/2014' 'delete059' 'no-asignations'

# TEST 60 --- delete060 --- Delete by estimated init date (valid) (** All selected task removed successfully)
cat > "$input/delete060.in" << \EOF
EOF
cat > "$output/delete060.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --est_start 05/05/2014' 'delete060'

# TEST 61 --- delete061 --- Delete by estimated final date (valid) (** All selected task removed successfully)
cat > "$input/delete061.in" << \EOF
EOF
cat > "$output/delete061.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d  --est_end 05/06/2014' 'delete061'

# TEST 62 --- delete062 --- Delete by real init date (valid) (** All selected task removed successfully)
cat > "$input/delete062.in" << \EOF
EOF
cat > "$output/delete062.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --start 05/08/2014' 'delete062'

# TEST 63 --- delete063 --- Delete by real final date (valid) (** All selected task removed successfully)
cat > "$input/delete063.in" << \EOF
EOF
cat > "$output/delete063.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --end 28/02/2014' 'delete063'

# TEST 64 --- delete064 --- Delete by priority (inexistent prior)
cat > "$input/delete064.in" << \EOF
EOF
cat > "$output/delete064.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d -p inexistent' 'delete064'

# TEST 65 --- delete065 --- Delete by priority (valid prior) (minus)
cat > "$input/delete065.in" << \EOF
EOF
cat > "$output/delete065.out" << \EOF
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p "very high"' 'delete065' 'no-asignations'

# TEST 66 --- delete066 --- Delete by priority (valid prior) (no tasks) (minus)
cat > "$input/delete066.in" << \EOF
EOF
cat > "$output/delete066.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p blocker' 'delete066'

# TEST 67 --- delete067 --- Delete by priority (valid prior) (mayus)
cat > "$input/delete067.in" << \EOF
EOF
cat > "$output/delete067.out" << \EOF
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p "VERY HIGH"' 'delete067' 'no-asignations'

# TEST 68 --- delete068 --- Delete by priority (valid prior) (no tasks) (mayus)
cat > "$input/delete068.in" << \EOF
EOF
cat > "$output/delete068.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p BLOCKER' 'delete068'

# TEST 69 --- delete069 --- Delete by priority (valid prior) (mixed [mayus | minus] )
cat > "$input/delete069.in" << \EOF
EOF
cat > "$output/delete069.out" << \EOF
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p "veRy HiGh"' 'delete069' 'no-asignations'

# TEST 70 --- delete070 --- Delete by priority (valid prior) (no tasks) (mixed [mayus | minus])
cat > "$input/delete070.in" << \EOF
EOF
cat > "$output/delete070.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p bLocKEr' 'delete070'

# TEST 71 --- delete071 --- Delete by type (inexistent type)
cat > "$input/delete071.in" << \EOF
EOF
cat > "$output/delete071.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d -t inexistent' 'delete071'

# TEST 72 --- delete072 --- Delete by type (valid type) (minus)
cat > "$input/delete072.in" << \EOF
EOF
cat > "$output/delete072.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t test' 'delete072' 'no-asignations'

# TEST 73 --- delete073 --- Delete by type (valid type) (no tasks) (minus)
cat > "$input/delete073.in" << \EOF
EOF
cat > "$output/delete073.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t support' 'delete073'

# TEST 74 --- delete074 --- Delete by type (valid type) (mayus)
cat > "$input/delete074.in" << \EOF
EOF
cat > "$output/delete074.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t TEST' 'delete074' 'no-asignations'

# TEST 75 --- delete075 --- Delete by type (valid type) (no tasks) (mayus)
cat > "$input/delete075.in" << \EOF
EOF
cat > "$output/delete075.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t SUPPORT' 'delete075'

# TEST 76 --- delete076 --- Delete by type (valid type) (mixed [mayus | minus] )
cat > "$input/delete076.in" << \EOF
EOF
cat > "$output/delete076.out" << \EOF
- Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t tEsT' 'delete076' 'no-asignations'

# TEST 77 --- delete077 --- Delete by type (valid type) (no tasks) (mixed [mayus | minus])
cat > "$input/delete077.in" << \EOF
EOF
cat > "$output/delete077.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t sUPpoRt' 'delete077'

# TEST 78 --- delete078 --- Delete by estimated time (invalid data)
cat > "$input/delete078.in" << \EOF
EOF
cat > "$output/delete078.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_time invalid' 'delete078'

# TEST 79 --- delete079 --- Delete by real time (invalid data)
cat > "$input/delete079.in" << \EOF
EOF
cat > "$output/delete079.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --time invalid' 'delete079'

# TEST 80 --- delete080 --- Delete by estimated time (valid data)
cat > "$input/delete080.in" << \EOF
EOF
cat > "$output/delete080.out" << \EOF
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --est_time 18' 'delete080' 'no-asignations'

# TEST 81 --- delete081 --- Delete by real time (valid data)
cat > "$input/delete081.in" << \EOF
EOF
cat > "$output/delete081.out" << \EOF
- Removing asignations and asociations to task 4 ...
- Task 4 prepared to be removed
- Removing asignations and asociations to task 7 ...
- Task 7 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --time 29' 'delete081'

# TEST 82 --- delete082 --- Delete by estimated time (valid data number) (overflow [more than 10 digits])
cat > "$input/delete082.in" << \EOF
EOF
cat > "$output/delete082.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_time 12345678901' 'delete082'

# TEST 83 --- delete083 --- Delete by real time (valid data number) (overflow [more than 10 digits])
cat > "$input/delete083.in" << \EOF
EOF
cat > "$output/delete083.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --time 12345678901' 'delete083'

# TEST 84 --- delete084 --- Delete by estimated time (valid data number) (zero)
cat > "$input/delete084.in" << \EOF
EOF
cat > "$output/delete084.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_time 0' 'delete084'

# TEST 85 --- delete085 --- Delete by real time (valid data number) (zero)
cat > "$input/delete085.in" << \EOF
EOF
cat > "$output/delete085.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --time 0' 'delete085'

# TEST 86 --- delete086 --- Delete by estimated time (valid data number) (negative limit -1)
cat > "$input/delete086.in" << \EOF
EOF
cat > "$output/delete086.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_time -1' 'delete086'

# TEST 87 --- delete087 --- Delete by real time (valid data number) (negative limit -1)
cat > "$input/delete087.in" << \EOF
EOF
cat > "$output/delete087.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --time -1' 'delete087'

# TEST 88 --- delete088 --- Delete by estimated time (valid data number) (negative other)
cat > "$input/delete088.in" << \EOF
EOF
cat > "$output/delete088.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --est_time -18' 'delete088'

# TEST 89 --- delete089 --- Delete by real time (valid data number) (negative other)
cat > "$input/delete089.in" << \EOF
EOF
cat > "$output/delete089.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --time -29' 'delete089'

# TEST 90 --- delete090 --- Delete by estimated time (valid data number) (no tasks)
cat > "$input/delete090.in" << \EOF
EOF
cat > "$output/delete090.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --est_time 46' 'delete090'

# TEST 91 --- delete091 --- Delete by real time (valid data number) (no tasks)
cat > "$input/delete091.in" << \EOF
EOF
cat > "$output/delete091.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --time 84' 'delete091'

# TEST 92 --- delete092 --- Delete task by state (valid state) (no tasks match) (minus)
cat > "$input/delete092.in" << \EOF
EOF
cat > "$output/delete092.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s resolved' 'delete092'

# TEST 93 --- delete093 --- Delete task by state (valid state) (no tasks match) (mayus)
cat > "$input/delete093.in" << \EOF
EOF
cat > "$output/delete093.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s RESOLVED' 'delete093'

# TEST 94 --- delete094 --- Delete task by state (valid state) (no tasks match) (mixed [mayus | minus])
cat > "$input/delete094.in" << \EOF
EOF
cat > "$output/delete094.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s ResOLvEd' 'delete094'

# TEST 95 --- delete095 --- Delete all tasks (only one in database)
cat > "$input/delete095.in" << \EOF
EOF
cat > "$output/delete095.out" << \EOF
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d' 'delete095' 'only-one'

# TEST 96 --- delete096 --- Delete tasks by name (multiple tasks with same name)
cat > "$input/delete096.in" << \EOF
EOF
cat > "$output/delete096.out" << \EOF
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
- Removing asignations and asociations to task 7 ...
- Task 7 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -n "same name"' 'delete096'


# TEST 97 --- delete097 --- Delete all tasks (empty database)
cat > "$input/delete097.in" << \EOF
EOF
cat > "$output/delete097.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d' 'delete097' 'empty'

# TEST 98 --- delete098 --- Delete task by valid state (empty database) (minus)
cat > "$input/delete098.in" << \EOF
EOF
cat > "$output/delete098.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s new' 'delete098' 'empty'

# TEST 99 --- delete099 --- Delete task by valid state (empty database) (mayus)
cat > "$input/delete099.in" << \EOF
EOF
cat > "$output/delete099.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s NEW' 'delete099' 'empty'

# TEST 100 --- delete100 --- Delete task by valid state (empty database) (mixed [mayus | minus])
cat > "$input/delete100.in" << \EOF
EOF
cat > "$output/delete100.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -s New' 'delete100' 'empty'

# TEST 101 --- delete101 --- Delete task by valid priority (empty database) (minus)
cat > "$input/delete101.in" << \EOF
EOF
cat > "$output/delete101.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p high' 'delete101' 'empty'

# TEST 102 --- delete102 --- Delete task by valid priority (empty database) (mayus)
cat > "$input/delete102.in" << \EOF
EOF
cat > "$output/delete102.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p HIGH' 'delete102' 'empty'

# TEST 103 --- delete103 --- Delete task by valid priority (empty database) (mixed [mayus | minus])
cat > "$input/delete103.in" << \EOF
EOF
cat > "$output/delete103.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -p HiGh' 'delete103' 'empty'

# TEST 104 --- delete104 --- Delete task by valid type (empty database) (mixed [mayus | minus])
cat > "$input/delete104.in" << \EOF
EOF
cat > "$output/delete104.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t TeST' 'delete104' 'empty'

# TEST 105 --- delete105 --- Delete task by valid type (empty database) (mayus)
cat > "$input/delete105.in" << \EOF
EOF
cat > "$output/delete105.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t TEST' 'delete105' 'empty'

# TEST 106 --- delete106 --- Delete task by valid type (empty database) (minus)
cat > "$input/delete106.in" << \EOF
EOF
cat > "$output/delete106.out" << \EOF
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d -t test' 'delete106' 'empty'

# SOME TESTS WITH ASOCIATIONS AND ASIGNATIONS IN DATABASE

# TEST 107 --- delete107 --- Delete all task (no filters) (multiple tasks exists)
cat > "$input/delete107.in" << \EOF
EOF
cat > "$output/delete107.out" << \EOF
- Removing asignations and asociations to task 1 ...
+ Selected file 'ruta1'
- Deasociated file 'ruta1'
+ Selected file 'rutaA'
- Deasociated file 'rutaA'
- Deasigned user usertest
- Task 1 prepared to be removed
- Removing asignations and asociations to task 2 ...
+ Selected file 'rutaB'
- Deasociated file 'rutaB'
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
Has found more than one path for file or folder 'mytest3'
Select one [0 - 1] and press ENTER
0 | rutaC
1 | rutaCbis
+ Selected file 'rutaC'
- Deasociated file 'rutaC'
+ Selected file 'rutaCbis'
- Deasociated file 'rutaCbis'
- Deasigned user usertest
- Task 3 prepared to be removed
- Removing asignations and asociations to task 4 ...
+ Selected file 'rutaB'
- Deasociated file 'rutaB'
+ Selected file 'rutaA'
- Deasociated file 'rutaA'
- Task 4 prepared to be removed
- Removing asignations and asociations to task 5 ...
- Deasigned user usertest
- Task 5 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
- Removing asignations and asociations to task 7 ...
- Task 7 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d' 'delete107' 'test-asociations'
