#!/bin/bash

source constants.sh

###########################
# 	TASK SEARCH TESTS
###########################
echo "testing: git task -r"


# TEST 1 --- read001 --- Search all task (no filters) (multiple tasks exists)
cat > "$input/read001.in" << \EOF
EOF
cat > "$output/read001.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
4 | Name: task 4   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
7 | Name: same name   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r' 'read001'

# TEST 2 --- read002 --- Search one task (id filter) (task exists)
cat > "$input/read002.in" << \EOF
EOF
cat > "$output/read002.out" << \EOF
Tasks found
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
EOF
./launch-test.sh 'git task -r --id 3' 'read002'

# TEST 3 --- read003 --- Search task by name (had to match exactly) (one task finded)
cat > "$input/read003.in" << \EOF
EOF
cat > "$output/read003.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r -n "task 5"' 'read003'

# TEST 4 --- read004 --- Search task by state (multiple task matches) (minus)
cat > "$input/read004.in" << \EOF
EOF
cat > "$output/read004.out" << \EOF
Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r -s "in progress"' 'read004'

# TEST 5 --- read005 --- Search task by state (multiple task matches) (mayus)
cat > "$input/read005.in" << \EOF
EOF
cat > "$output/read005.out" << \EOF
Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
	Start	Estimated: empty	Real: 21/12/2014
	End  	Estimated: 24/12/2014	Real: empty
	Time 	Estimated: -1	Real: 12
	No assigned yet
	No associated files yet
	Description: empty
	Notes: my personal notes
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 26/12/2014	Real: 28/12/2014
	Time 	Estimated: 18	Real: -1
	Assigned to: usertest   	No associated files yet
	Description: empty
	Notes: empty
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
	Start	Estimated: empty	Real: 21/12/2014
	End  	Estimated: 24/12/2014	Real: empty
	Time 	Estimated: -1	Real: 12
	No assigned yet
	No associated files yet
	Description: empty
	Notes: my personal notes
EOF
./launch-test.sh 'git task -r -s "IN PROGRESS" -v' 'read005'

# TEST 6 --- read006 --- Search task by state (multiple task matches) (mayus & minus)
cat > "$input/read006.in" << \EOF
EOF
cat > "$output/read006.out" << \EOF
Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r -s "iN proGrEss"' 'read006'

# TEST 7 --- read007 --- Search by id (invalid id)
cat > "$input/read007.in" << \EOF
EOF
cat > "$output/read007.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --id hola' 'read007'

# TEST 8 --- read008 --- Search by id (no task matching)
cat > "$input/read008.in" << \EOF
EOF
cat > "$output/read008.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r --id 20' 'read008'

# TEST 9 --- read009 --- Search by name (only part of name matches)
cat > "$input/read009.in" << \EOF
EOF
cat > "$output/read009.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -n "inexistent"' 'read009'

# TEST 10 --- read010 --- Search task by name (part of name matching only with one task)
cat > "$input/read010.in" << \EOF
EOF
cat > "$output/read010.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r -n 5' 'read010'

# TEST 11 --- read011 --- Search task by name (part of name matching with multiple task names)
cat > "$input/read011.in" << \EOF
EOF
cat > "$output/read011.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
4 | Name: task 4   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r -n task' 'read011'

# TEST 12 --- read012 --- Search task by state (only one matching) (minus)
cat > "$input/read012.in" << \EOF
EOF
cat > "$output/read012.out" << \EOF
Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r -s new' 'read012'

# TEST 13 --- read013 --- Search task by state (only one matching) (mayus)
cat > "$input/read013.in" << \EOF
EOF
cat > "$output/read013.out" << \EOF
Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r -s NEW' 'read013'

# TEST 14 --- read014 --- Search task by state (only one matching) (mixed mayus and minus)
cat > "$input/read014.in" << \EOF
EOF
cat > "$output/read014.out" << \EOF
Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r -s New' 'read014'

# TEST 15 --- read015 --- Search task by state (invalid / inexistent state)
cat > "$input/read015.in" << \EOF
EOF
cat > "$output/read015.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r -s inexistent' 'read015'

# TEST 16 --- read016 --- Search task by estimated init date (invalid date letters)
cat > "$input/read016.in" << \EOF
EOF
cat > "$output/read016.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start date' 'read016'

# TEST 17 --- read017 --- Search task by estimated init date (invalid day in date)
cat > "$input/read017.in" << \EOF
EOF
cat > "$output/read017.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 50/12/2014' 'read017'

# TEST 18 --- read018 --- Search task by estimated init date (invalid month in date)
cat > "$input/read018.in" << \EOF
EOF
cat > "$output/read018.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 15/16/2014' 'read018'

# TEST 19 --- read019 --- Search task by estimated init date (invalid year in date)
cat > "$input/read019.in" << \EOF
EOF
cat > "$output/read019.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 15/05/23' 'read019'

# TEST 20 --- read020 --- Search task by estimated init date (invalid day in date) (negative)
cat > "$input/read020.in" << \EOF
EOF
cat > "$output/read020.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start -12/06/2014' 'read020'

# TEST 21 --- read021 --- Search task by estimated init date (invalid day in date) (zero)
cat > "$input/read021.in" << \EOF
EOF
cat > "$output/read021.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 00/06/2014' 'read021'

# TEST 22 --- read022 --- Search task by estimated  init date (invalid month in date) (zero)
cat > "$input/read022.in" << \EOF
EOF
cat > "$output/read022.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 15/00/2014' 'read022'

# TEST 23 --- read023 --- Search task by estimated init date (invalid month in date) (negative)
cat > "$input/read023.in" << \EOF
EOF
cat > "$output/read023.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 15/-11/2014' 'read023'

# TEST 24 --- read024 --- Search task by estimated init date (invalid year in date) (over four numbers)
cat > "$input/read024.in" << \EOF
EOF
cat > "$output/read024.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 15/05/20146' 'read024'

# TEST 25 --- read025 --- Search task by estimated init date (invalid year in date) (negative)
cat > "$input/read025.in" << \EOF
EOF
cat > "$output/read025.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_start 15/11/-2014' 'read025'

# TEST 26 --- read026 --- Search task by estimated final date (invalid date letters)
cat > "$input/read026.in" << \EOF
EOF
cat > "$output/read026.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end date' 'read026'

# TEST 27 --- read027 --- Search task by estimated final date (invalid day in date)
cat > "$input/read027.in" << \EOF
EOF
cat > "$output/read027.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 50/12/2014' 'read027'

# TEST 28 --- read028 --- Search task by estimated final date (invalid month in date)
cat > "$input/read028.in" << \EOF
EOF
cat > "$output/read028.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 15/16/2014' 'read028'

# TEST 29 --- read029 --- Search task by estimated final date (invalid year in date)
cat > "$input/read029.in" << \EOF
EOF
cat > "$output/read029.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 15/05/23' 'read029'

# TEST 30 --- read030 --- Search task by estimated final date (invalid day in date) (negative)
cat > "$input/read030.in" << \EOF
EOF
cat > "$output/read030.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end -12/10/2014' 'read030'

# TEST 31 --- read031 --- Search task by estimated final date (invalid day in date) (zero)
cat > "$input/read031.in" << \EOF
EOF
cat > "$output/read031.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 00/05/2014' 'read031'

# TEST 32 --- read032 --- Search task by estimated final date (invalid month in date) (zero)
cat > "$input/read032.in" << \EOF
EOF
cat > "$output/read032.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 15/00/214' 'read032'

# TEST 33 --- read033 --- Search task by estimated final date (invalid month in date) (negative)
cat > "$input/read033.in" << \EOF




15/-11/2014






EOF
cat > "$output/read033.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 15/-11/2014' 'read033'

# TEST 34 --- read034 --- Search task by estimated final date (invalid year in date) (over four numbers)
cat > "$input/read034.in" << \EOF
EOF
cat > "$output/read034.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 15/05/20146' 'read034'

# TEST 35 --- read035 --- Search task by estimated final date (invalid year in date) (negative)
cat > "$input/read035.in" << \EOF
EOF
cat > "$output/read035.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_end 15/11/-2014' 'read035'

# TEST 36 --- read036 --- Search task by real init date (invalid date letters)
cat > "$input/read036.in" << \EOF
EOF
cat > "$output/read036.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start date' 'read036'

# TEST 37 --- read037 --- Search task by real init date (invalid day in date)
cat > "$input/read037.in" << \EOF
EOF
cat > "$output/read037.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 50/12/2014' 'read037'

# TEST 38 --- read038 --- Search task by real init date (invalid month in date)
cat > "$input/read038.in" << \EOF
EOF
cat > "$output/read038.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 15/16/2014' 'read038'

# TEST 39 --- read039 --- Search task by real init date (invalid year in date)
cat > "$input/read039.in" << \EOF
EOF
cat > "$output/read039.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 15/05/23' 'read039'

# TEST 40 --- read040 --- Search task by real init date (invalid day in date) (negative)
cat > "$input/read040.in" << \EOF
EOF
cat > "$output/read040.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start -12/10/2014' 'read040'

# TEST 41 --- read041 --- Search task by real init date (invalid day in date) (zero)
cat > "$input/read041.in" << \EOF
EOF
cat > "$output/read041.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 00/11/2014' 'read041'

# TEST 42 --- read042 --- Search task by real init date (invalid month in date) (zero)
cat > "$input/read042.in" << \EOF
EOF
cat > "$output/read042.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 15/00/2014' 'read042'

# TEST 43 --- read043 --- Search task by real init date (invalid month in date) (negative)
cat > "$input/read043.in" << \EOF
EOF
cat > "$output/read043.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 15/-11/2014' 'read043'

# TEST 44 --- read044 --- Search task by real init date (invalid year in date) (over four numbers)
cat > "$input/read044.in" << \EOF
EOF
cat > "$output/read044.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 15/05/20146' 'read044'

# TEST 45 --- read045 --- Search task by real init date (invalid year in date) (negative)
cat > "$input/read045.in" << \EOF
EOF
cat > "$output/read045.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --start 15/11/-2014' 'read045'

# TEST 46 --- read046 --- Search task by real final date (invalid date letters)
cat > "$input/read046.in" << \EOF
EOF
cat > "$output/read046.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end date' 'read046'

# TEST 47 --- read047 --- Search task by real final date (invalid day in date)
cat > "$input/read047.in" << \EOF
EOF
cat > "$output/read047.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 50/12/2014' 'read047'

# TEST 48 --- read048 --- Search task by real final date (invalid month in date)
cat > "$input/read048.in" << \EOF
EOF
cat > "$output/read048.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 15/16/2014' 'read048'

# TEST 49 --- read049 --- Search task by real final date (invalid year in date)
cat > "$input/read049.in" << \EOF
EOF
cat > "$output/read049.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 15/05/23' 'read049'

# TEST 50 --- read050 --- Search task by real final date (invalid day in date) (negative)
cat > "$input/read050.in" << \EOF
EOF
cat > "$output/read050.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end -12/10/2014' 'read050'

# TEST 51 --- read051 --- Search task by real final date (invalid day in date) (zero)
cat > "$input/read051.in" << \EOF
EOF
cat > "$output/read051.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 00/11/2014' 'read051'

# TEST 52 --- read052 --- Search task by real final date (invalid month in date) (zero)
cat > "$input/read052.in" << \EOF
EOF
cat > "$output/read052.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 15/00/2014' 'read052'

# TEST 53 --- read053 --- Search task by real final date (invalid month in date) (negative)
cat > "$input/read053.in" << \EOF
EOF
cat > "$output/read053.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 15/-11/2014' 'read053'

# TEST 54 --- read054 --- Search task by real final date (invalid year in date) (over four numbers)
cat > "$input/read054.in" << \EOF
EOF
cat > "$output/read054.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 15/05/20146' 'read054'

# TEST 55 --- read055 --- Search task by real final date (invalid year in date) (negative)
cat > "$input/read055.in" << \EOF
EOF
cat > "$output/read055.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --end 15/11/-2014' 'read055'

# TEST 56 --- read056 --- Search by estimated init date (valid)
cat > "$input/read056.in" << \EOF
EOF
cat > "$output/read056.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r --est_start 30/12/2014' 'read056'

# TEST 57 --- read057 --- Search by estimated final date (valid)
cat > "$input/read057.in" << \EOF
EOF
cat > "$output/read057.out" << \EOF
Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r --est_end 24/12/2014' 'read057'

# TEST 58 --- read058 --- Search by real init date (valid)
cat > "$input/read058.in" << \EOF
EOF
cat > "$output/read058.out" << \EOF
Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r --start 21/12/2014' 'read058'

# TEST 59 --- read059 --- Search by real final date (valid) (one task matching)
cat > "$input/read059.in" << \EOF
EOF
cat > "$output/read059.out" << \EOF
Tasks found
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
EOF
./launch-test.sh 'git task -r --end 28/12/2014' 'read059'

# TEST 60 --- read060 --- Search by estimated init date (valid) (no task matching)
cat > "$input/read060.in" << \EOF
EOF
cat > "$output/read060.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r --est_start 05/05/2014' 'read060'

# TEST 61 --- read061 --- Search by estimated final date (valid) (no task matching)
cat > "$input/read061.in" << \EOF
EOF
cat > "$output/read061.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r --est_end 05/06/2014' 'read061'

# TEST 62 --- read062 --- Search by real init date (valid) (no task matching)
cat > "$input/read062.in" << \EOF
EOF
cat > "$output/read062.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r --start 05/08/2014' 'read062'

# TEST 63 --- read063 --- Search by real final date (valid) (no task matching)
cat > "$input/read063.in" << \EOF
EOF
cat > "$output/read063.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r --end 28/02/2014' 'read063'

# TEST 64 --- read064 --- Search by priority (inexistent prior)
cat > "$input/read064.in" << \EOF
EOF
cat > "$output/read064.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r -p inexistent' 'read064'

# TEST 65 --- read065 --- Search by priority (valid prior) (minus)
cat > "$input/read065.in" << \EOF
EOF
cat > "$output/read065.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r -p "very high"' 'read065'

# TEST 66 --- read066 --- Search by priority (valid prior) (no tasks) (minus)
cat > "$input/read066.in" << \EOF
EOF
cat > "$output/read066.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -p "blocker"' 'read066'

# TEST 67 --- read067 --- Search by priority (valid prior) (mayus)
cat > "$input/read067.in" << \EOF
EOF
cat > "$output/read067.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r -p "VERY HIGH"' 'read067'

# TEST 68 --- read068 --- Search by priority (valid prior) (no tasks) (mayus)
cat > "$input/read068.in" << \EOF
EOF
cat > "$output/read068.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -p BLOCKER' 'read068'

# TEST 69 --- read069 --- Search by priority (valid prior) (mixed [mayus | minus] )
cat > "$input/read069.in" << \EOF
EOF
cat > "$output/read069.out" << \EOF
Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r -p "veRy hIgH"' 'read069'

# TEST 70 --- read070 --- Search by priority (valid prior) (no tasks) (mixed [mayus | minus])
cat > "$input/read070.in" << \EOF
EOF
cat > "$output/read070.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -p blOcKEr' 'read070'

# TEST 71 --- read071 --- Search by type (inexistent type)
cat > "$input/read071.in" << \EOF
EOF
cat > "$output/read071.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r -t inexistent' 'read071'

# TEST 72 --- read072 --- Search by type (valid type) (minus)
cat > "$input/read072.in" << \EOF
EOF
cat > "$output/read072.out" << \EOF
Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r -t test' 'read072'

# TEST 73 --- read073 --- Search by type (valid type) (no tasks) (minus)
cat > "$input/read073.in" << \EOF
EOF
cat > "$output/read073.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -t support' 'read073'

# TEST 74 --- read074 --- Search by type (valid type) (mayus)
cat > "$input/read074.in" << \EOF
EOF
cat > "$output/read074.out" << \EOF
Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r -t TEST' 'read074'

# TEST 75 --- read075 --- Search by type (valid type) (no tasks) (mayus)
cat > "$input/read075.in" << \EOF
EOF
cat > "$output/read075.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -t SUPPORT' 'read075'

# TEST 76 --- read076 --- Search by type (valid type) (mixed [mayus | minus] )
cat > "$input/read076.in" << \EOF
EOF
cat > "$output/read076.out" << \EOF
Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r -t tEsT' 'read076'

# TEST 77 --- read077 --- Search by type (valid type) (no tasks) (mixed [mayus | minus])
cat > "$input/read077.in" << \EOF
EOF
cat > "$output/read077.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -t suPPorT' 'read077'

# TEST 78 --- read078 --- Search by estimated time (invalid data)
cat > "$input/read078.in" << \EOF
EOF
cat > "$output/read078.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_time invalid' 'read078'

# TEST 79 --- read079 --- Search by real time (invalid data)
cat > "$input/read079.in" << \EOF
EOF
cat > "$output/read079.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --time invalid' 'read079'

# TEST 80 --- read080 --- Search by estimated time (valid data)
cat > "$input/read080.in" << \EOF
EOF
cat > "$output/read080.out" << \EOF
Tasks found
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
EOF
./launch-test.sh 'git task -r --est_time 18' 'read080'

# TEST 81 --- read081 --- Search by real time (valid data)
cat > "$input/read081.in" << \EOF
EOF
cat > "$output/read081.out" << \EOF
Tasks found
4 | Name: task 4   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
7 | Name: same name   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r --time 29' 'read081'

# TEST 82 --- read082 --- Search by estimated time (valid data number) (overflow [more than 10 digits])
cat > "$input/read082.in" << \EOF
EOF
cat > "$output/read082.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_time 12345678901' 'read082'

# TEST 83 --- read083 --- Search by real time (valid data number) (overflow [more than 10 digits])
cat > "$input/read083.in" << \EOF
EOF
cat > "$output/read083.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --time 12345678901' 'read083'

# TEST 84 --- read084 --- Search by estimated time (valid data number) (zero)
cat > "$input/read084.in" << \EOF
EOF
cat > "$output/read084.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_time 0' 'read084'

# TEST 85 --- read085 --- Search by real time (valid data number) (zero)
cat > "$input/read085.in" << \EOF
EOF
cat > "$output/read085.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --time 0' 'read085'

# TEST 86 --- read086 --- Search by estimated time (valid data number) (negative limit -1)
cat > "$input/read086.in" << \EOF
EOF
cat > "$output/read086.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_time -1' 'read086'

# TEST 87 --- read087 --- Search by real time (valid data number) (negative limit -1)
cat > "$input/read087.in" << \EOF
EOF
cat > "$output/read087.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --time -1' 'read087'

# TEST 88 --- read088 --- Search by estimated time (valid data number) (negative other)
cat > "$input/read088.in" << \EOF
EOF
cat > "$output/read088.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --est_time -18' 'read088'

# TEST 89 --- read089 --- Search by real time (valid data number) (negative other)
cat > "$input/read089.in" << \EOF
EOF
cat > "$output/read089.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --time -29' 'read089'

# TEST 90 --- read090 --- Search by estimated time (valid data number) (no tasks)
cat > "$input/read090.in" << \EOF
EOF
cat > "$output/read090.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r --est_time 46' 'read090'

# TEST 91 --- read091 --- Search by real time (valid data number) (no tasks)
cat > "$input/read091.in" << \EOF
EOF
cat > "$output/read091.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r --time 84' 'read091'

# TEST 92 --- read092 --- Search task by state (valid state) (no tasks match) (minus)
cat > "$input/read092.in" << \EOF
EOF
cat > "$output/read092.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -s resolved' 'read092'

# TEST 93 --- read093 --- Search task by state (valid state) (no tasks match) (mayus)
cat > "$input/read093.in" << \EOF
EOF
cat > "$output/read093.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -s RESOLVED' 'read093'

# TEST 94 --- read094 --- Search task by state (valid state) (no tasks match) (mixed [mayus | minus])
cat > "$input/read094.in" << \EOF
EOF
cat > "$output/read094.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -s ResOlVEd' 'read094'

# TEST 95 --- read095 --- Search all tasks (only one in database)
cat > "$input/read095.in" << \EOF
EOF
cat > "$output/read095.out" << \EOF
Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r' 'read095' 'only-one'

# TEST 96 --- read096 --- Search tasks by name (multiple tasks with same name)
cat > "$input/read096.in" << \EOF
EOF
cat > "$output/read096.out" << \EOF
Tasks found
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
7 | Name: same name   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r -n "same name"' 'read096'

# TEST 97 --- read097 --- Search all tasks (empty database)
cat > "$input/read097.in" << \EOF
EOF
cat > "$output/read097.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r' 'read097' 'empty'

# TEST 98 --- read098 --- Search task by valid state (empty database) (minus)
cat > "$input/read098.in" << \EOF
EOF
cat > "$output/read098.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -s new' 'read098' 'empty'

# TEST 99 --- read099 --- Search task by valid state (empty database) (mayus)
cat > "$input/read099.in" << \EOF
EOF
cat > "$output/read099.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -s NEW' 'read099' 'empty'

# TEST 100 --- read100 --- Search task by valid state (empty database) (mixed [mayus | minus])
cat > "$input/read100.in" << \EOF
EOF
cat > "$output/read100.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -s nEw' 'read100' 'empty'

# TEST 101 --- read101 --- Search task by valid priority (empty database) (minus)
cat > "$input/read101.in" << \EOF
EOF
cat > "$output/read101.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -p high' 'read101' 'empty'

# TEST 102 --- read102 --- Search task by valid priority (empty database) (mayus)
cat > "$input/read102.in" << \EOF
EOF
cat > "$output/read102.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -p HIGH' 'read102' 'empty'

# TEST 103 --- read103 --- Search task by valid priority (empty database) (mixed [mayus | minus])
cat > "$input/read103.in" << \EOF
EOF
cat > "$output/read103.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -p hIGh' 'read103' 'empty'

# TEST 104 --- read104 --- Search task by valid type (empty database) (mixed [mayus | minus])
cat > "$input/read104.in" << \EOF
EOF
cat > "$output/read104.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -t tEsT' 'read104' 'empty'

# TEST 105 --- read105 --- Search task by valid type (empty database) (mayus)
cat > "$input/read105.in" << \EOF
EOF
cat > "$output/read105.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -t TEST' 'read105' 'empty'

# TEST 106 --- read106 --- Search task by valid type (empty database) (minus)
cat > "$input/read106.in" << \EOF
EOF
cat > "$output/read106.out" << \EOF
No task matching
EOF
./launch-test.sh 'git task -r -t test' 'read106' 'empty'
