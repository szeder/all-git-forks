#!/bin/bash

source constants.sh

###########################
# 	TASK SEARCH TESTS
###########################
echo "testing: git task -r --assist"


# TEST 1 --- assistread001 --- Search all task (no filters) (multiple tasks exists)
cat > "$input/assistread001.in" << \EOF











EOF
cat > "$output/assistread001.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
4 | Name: task 4   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
7 | Name: same name   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread001'

# TEST 2 --- assistread002 --- Search one task (id filter) (task exists)
cat > "$input/assistread002.in" << \EOF
3










EOF
cat > "$output/assistread002.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread002'

# TEST 3 --- assistread003 --- Search task by name (had to match exactly) (one task finded)
cat > "$input/assistread003.in" << \EOF

task 5









EOF
cat > "$output/assistread003.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread003'

# TEST 4 --- assistread004 --- Search task by state (multiple task matches) (minus)
cat > "$input/assistread004.in" << \EOF


in progress








EOF
cat > "$output/assistread004.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread004'

# TEST 5 --- assistread005 --- Search task by state (multiple task matches) (mayus)
cat > "$input/assistread005.in" << \EOF


IN PROGRESS








EOF
cat > "$output/assistread005.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
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
./launch-test.sh 'git task -r --assist --assist -v' 'assistread005'

# TEST 6 --- assistread006 --- Search task by state (multiple task matches) (mayus & minus)
cat > "$input/assistread006.in" << \EOF


iN proGrEss








EOF
cat > "$output/assistread006.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread006'

# TEST 7 --- assistread007 --- Search by id (invalid id)
cat > "$input/assistread007.in" << \EOF
hola










EOF
cat > "$output/assistread007.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread007'

# TEST 8 --- assistread008 --- Search by id (no task matching)
cat > "$input/assistread008.in" << \EOF
20










EOF
cat > "$output/assistread008.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread008'

# TEST 9 --- assistread009 --- Search by name (only part of name matches)
cat > "$input/assistread009.in" << \EOF

inexistent









EOF
cat > "$output/assistread009.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist --assist' 'assistread009'

# TEST 10 --- assistread010 --- Search task by name (part of name matching only with one task)
cat > "$input/assistread010.in" << \EOF

5









EOF
cat > "$output/assistread010.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r --assist' 'assistread010'

# TEST 11 --- assistread011 --- Search task by name (part of name matching with multiple task names)
cat > "$input/assistread011.in" << \EOF

task









EOF
cat > "$output/assistread011.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
4 | Name: task 4   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r --assist' 'assistread011'

# TEST 12 --- assistread012 --- Search task by state (only one matching) (minus)
cat > "$input/assistread012.in" << \EOF


new








EOF
cat > "$output/assistread012.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r --assist' 'assistread012'

# TEST 13 --- assistread013 --- Search task by state (only one matching) (mayus)
cat > "$input/assistread013.in" << \EOF


NEW








EOF
cat > "$output/assistread013.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r --assist' 'assistread013'

# TEST 14 --- assistread014 --- Search task by state (only one matching) (mixed mayus and minus)
cat > "$input/assistread014.in" << \EOF


New








EOF
cat > "$output/assistread014.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r --assist' 'assistread014'

# TEST 15 --- assistread015 --- Search task by state (invalid / inexistent state)
cat > "$input/assistread015.in" << \EOF


inexistent








EOF
cat > "$output/assistread015.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread015'

# TEST 16 --- assistread016 --- Search task by estimated init date (invalid date letters)
cat > "$input/assistread016.in" << \EOF



date







EOF
cat > "$output/assistread016.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread016'

# TEST 17 --- assistread017 --- Search task by estimated init date (invalid day in date)
cat > "$input/assistread017.in" << \EOF



50/12/2014







EOF
cat > "$output/assistread017.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread017'

# TEST 18 --- assistread018 --- Search task by estimated init date (invalid month in date)
cat > "$input/assistread018.in" << \EOF



15/16/2014







EOF
cat > "$output/assistread018.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread018'

# TEST 19 --- assistread019 --- Search task by estimated init date (invalid year in date)
cat > "$input/assistread019.in" << \EOF



15/05/23







EOF
cat > "$output/assistread019.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread019'

# TEST 20 --- assistread020 --- Search task by estimated init date (invalid day in date) (negative)
cat > "$input/assistread020.in" << \EOF



-12/06/2014







EOF
cat > "$output/assistread020.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread020'

# TEST 21 --- assistread021 --- Search task by estimated init date (invalid day in date) (zero)
cat > "$input/assistread021.in" << \EOF



00/06/2014







EOF
cat > "$output/assistread021.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread021'

# TEST 22 --- assistread022 --- Search task by estimated  init date (invalid month in date) (zero)
cat > "$input/assistread022.in" << \EOF



15/00/2014







EOF
cat > "$output/assistread022.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread022'

# TEST 23 --- assistread023 --- Search task by estimated init date (invalid month in date) (negative)
cat > "$input/assistread023.in" << \EOF



15/-11/2014







EOF
cat > "$output/assistread023.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread023'

# TEST 24 --- assistread024 --- Search task by estimated init date (invalid year in date) (over four numbers)
cat > "$input/assistread024.in" << \EOF



15/05/20146







EOF
cat > "$output/assistread024.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread024'

# TEST 25 --- assistread025 --- Search task by estimated init date (invalid year in date) (negative)
cat > "$input/assistread025.in" << \EOF



15/11/-2014







EOF
cat > "$output/assistread025.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread025'

# TEST 26 --- assistread026 --- Search task by estimated final date (invalid date letters)
cat > "$input/assistread026.in" << \EOF




date






EOF
cat > "$output/assistread026.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread026'

# TEST 27 --- assistread027 --- Search task by estimated final date (invalid day in date)
cat > "$input/assistread027.in" << \EOF




50/12/2014







EOF
cat > "$output/assistread027.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread027'

# TEST 28 --- assistread028 --- Search task by estimated final date (invalid month in date)
cat > "$input/assistread028.in" << \EOF




15/16/2014







EOF
cat > "$output/assistread028.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread028'

# TEST 29 --- assistread029 --- Search task by estimated final date (invalid year in date)
cat > "$input/assistread029.in" << \EOF




15/05/23







EOF
cat > "$output/assistread029.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread029'

# TEST 30 --- assistread030 --- Search task by estimated final date (invalid day in date) (negative)
cat > "$input/assistread030.in" << \EOF




-12/10/2014






EOF
cat > "$output/assistread030.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread030'

# TEST 31 --- assistread031 --- Search task by estimated final date (invalid day in date) (zero)
cat > "$input/assistread031.in" << \EOF




00/05/2014






EOF
cat > "$output/assistread031.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread031'

# TEST 32 --- assistread032 --- Search task by estimated final date (invalid month in date) (zero)
cat > "$input/assistread032.in" << \EOF




15/00/2014






EOF
cat > "$output/assistread032.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread032'

# TEST 33 --- assistread033 --- Search task by estimated final date (invalid month in date) (negative)
cat > "$input/assistread033.in" << \EOF




15/-11/2014






EOF
cat > "$output/assistread033.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread033'

# TEST 34 --- assistread034 --- Search task by estimated final date (invalid year in date) (over four numbers)
cat > "$input/assistread034.in" << \EOF




15/05/20146






EOF
cat > "$output/assistread034.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread034'

# TEST 35 --- assistread035 --- Search task by estimated final date (invalid year in date) (negative)
cat > "$input/assistread035.in" << \EOF




15/11/-2014






EOF
cat > "$output/assistread035.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread035'

# TEST 36 --- assistread036 --- Search task by real init date (invalid date letters)
cat > "$input/assistread036.in" << \EOF





date





EOF
cat > "$output/assistread036.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread036'

# TEST 37 --- assistread037 --- Search task by real init date (invalid day in date)
cat > "$input/assistread037.in" << \EOF





50/12/2014






EOF
cat > "$output/assistread037.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread037'

# TEST 38 --- assistread038 --- Search task by real init date (invalid month in date)
cat > "$input/assistread038.in" << \EOF





15/16/2014






EOF
cat > "$output/assistread038.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread038'

# TEST 39 --- assistread039 --- Search task by real init date (invalid year in date)
cat > "$input/assistread039.in" << \EOF





15/05/23






EOF
cat > "$output/assistread039.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread039'

# TEST 40 --- assistread040 --- Search task by real init date (invalid day in date) (negative)
cat > "$input/assistread040.in" << \EOF





-12/10/2014





EOF
cat > "$output/assistread040.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread040'

# TEST 41 --- assistread041 --- Search task by real init date (invalid day in date) (zero)
cat > "$input/assistread041.in" << \EOF





00/11/2014





EOF
cat > "$output/assistread041.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread041'

# TEST 42 --- assistread042 --- Search task by real init date (invalid month in date) (zero)
cat > "$input/assistread042.in" << \EOF





15/00/2014





EOF
cat > "$output/assistread042.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread042'

# TEST 43 --- assistread043 --- Search task by real init date (invalid month in date) (negative)
cat > "$input/assistread043.in" << \EOF





15/-11/2014





EOF
cat > "$output/assistread043.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread043'

# TEST 44 --- assistread044 --- Search task by real init date (invalid year in date) (over four numbers)
cat > "$input/assistread044.in" << \EOF





15/05/20146





EOF
cat > "$output/assistread044.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread044'

# TEST 45 --- assistread045 --- Search task by real init date (invalid year in date) (negative)
cat > "$input/assistread045.in" << \EOF





15/11/-2014





EOF
cat > "$output/assistread045.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread045'

# TEST 46 --- assistread046 --- Search task by real final date (invalid date letters)
cat > "$input/assistread046.in" << \EOF






date




EOF
cat > "$output/assistread046.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread046'

# TEST 47 --- assistread047 --- Search task by real final date (invalid day in date)
cat > "$input/assistread047.in" << \EOF






50/12/2014





EOF
cat > "$output/assistread047.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread047'

# TEST 48 --- assistread048 --- Search task by real final date (invalid month in date)
cat > "$input/assistread048.in" << \EOF






15/16/2014





EOF
cat > "$output/assistread048.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread048'

# TEST 49 --- assistread049 --- Search task by real final date (invalid year in date)
cat > "$input/assistread049.in" << \EOF






15/05/23





EOF
cat > "$output/assistread049.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread049'

# TEST 50 --- assistread050 --- Search task by real final date (invalid day in date) (negative)
cat > "$input/assistread050.in" << \EOF






-12/10/2014




EOF
cat > "$output/assistread050.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread050'

# TEST 51 --- assistread051 --- Search task by real init date (invalid day in date) (zero)
cat > "$input/assistread051.in" << \EOF






00/11/2014




EOF
cat > "$output/assistread051.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread051'

# TEST 52 --- assistread052 --- Search task by real final date (invalid month in date) (zero)
cat > "$input/assistread052.in" << \EOF






15/00/2014




EOF
cat > "$output/assistread052.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread052'

# TEST 53 --- assistread053 --- Search task by real final date (invalid month in date) (negative)
cat > "$input/assistread053.in" << \EOF






15/-11/2014




EOF
cat > "$output/assistread053.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread053'

# TEST 54 --- assistread054 --- Search task by real final date (invalid year in date) (over four numbers)
cat > "$input/assistread054.in" << \EOF






15/05/20146




EOF
cat > "$output/assistread054.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread054'

# TEST 55 --- assistread055 --- Search task by real final date (invalid year in date) (negative)
cat > "$input/assistread055.in" << \EOF






15/11/-2014




EOF
cat > "$output/assistread055.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread055'

# TEST 56 --- assistread056 --- Search by estimated init date (valid) (one task matching)
cat > "$input/assistread056.in" << \EOF



30/12/2014







EOF
cat > "$output/assistread056.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r --assist' 'assistread056'

# TEST 57 --- assistread057 --- Search by estimated final date (valid)
cat > "$input/assistread057.in" << \EOF




24/12/2014






EOF
cat > "$output/assistread057.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r --assist' 'assistread057'

# TEST 58 --- assistread058 --- Search by real init date (valid)
cat > "$input/assistread058.in" << \EOF





21/12/2014





EOF
cat > "$output/assistread058.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r --assist' 'assistread058'

# TEST 59 --- assistread059 --- Search by real final date (valid) (one task matching)
cat > "$input/assistread059.in" << \EOF






28/12/2014




EOF
cat > "$output/assistread059.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
EOF
./launch-test.sh 'git task -r --assist' 'assistread059'

# TEST 60 --- assistread060 --- Search by estimated init date (valid) (no task matching)
cat > "$input/assistread060.in" << \EOF



05/05/2014







EOF
cat > "$output/assistread060.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread060'

# TEST 61 --- assistread061 --- Search by estimated final date (valid) (no task matching)
cat > "$input/assistread061.in" << \EOF




05/06/2014






EOF
cat > "$output/assistread061.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread061'

# TEST 62 --- assistread062 --- Search by real init date (valid) (no task matching)
cat > "$input/assistread062.in" << \EOF





05/08/2014





EOF
cat > "$output/assistread062.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread062'

# TEST 63 --- assistread063 --- Search by real final date (valid) (no task matching)
cat > "$input/assistread063.in" << \EOF






28/02/2014




EOF
cat > "$output/assistread063.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread063'

# TEST 64 --- assistread064 --- Search by priority (inexistent prior)
cat > "$input/assistread064.in" << \EOF







inexistent



EOF
cat > "$output/assistread064.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread064'

# TEST 65 --- assistread065 --- Search by priority (valid prior) (minus)
cat > "$input/assistread065.in" << \EOF







very high



EOF
cat > "$output/assistread065.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r --assist' 'assistread065'

# TEST 66 --- assistread066 --- Search by priority (valid prior) (no tasks) (minus)
cat > "$input/assistread066.in" << \EOF







blocker



EOF
cat > "$output/assistread066.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread066'

# TEST 67 --- assistread067 --- Search by priority (valid prior) (mayus)
cat > "$input/assistread067.in" << \EOF







VERY HIGH



EOF
cat > "$output/assistread067.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r --assist' 'assistread067'

# TEST 68 --- assistread068 --- Search by priority (valid prior) (no tasks) (mayus)
cat > "$input/assistread068.in" << \EOF







BLOCKER



EOF
cat > "$output/assistread068.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread068'

# TEST 69 --- assistread069 --- Search by priority (valid prior) (mixed [mayus | minus] )
cat > "$input/assistread069.in" << \EOF







veRy hIgH



EOF
cat > "$output/assistread069.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
8 | Name: task 8   State: REJECTED   Priority: VERY HIGH   Type: CONFIGURATION
EOF
./launch-test.sh 'git task -r --assist' 'assistread069'

# TEST 70 --- assistread070 --- Search by priority (valid prior) (no tasks) (mixed [mayus | minus])
cat > "$input/assistread070.in" << \EOF







blOcKEr



EOF
cat > "$output/assistread070.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread070'

# TEST 71 --- assistread071 --- Search by type (inexistent type)
cat > "$input/assistread071.in" << \EOF








inexistent


EOF
cat > "$output/assistread071.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread071'

# TEST 72 --- assistread072 --- Search by type (valid type) (minus)
cat > "$input/assistread072.in" << \EOF








test


EOF
cat > "$output/assistread072.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r --assist' 'assistread072'

# TEST 73 --- assistread073 --- Search by type (valid type) (no tasks) (minus)
cat > "$input/assistread073.in" << \EOF








support


EOF
cat > "$output/assistread073.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread073'

# TEST 74 --- assistread074 --- Search by type (valid type) (mayus)
cat > "$input/assistread074.in" << \EOF








TEST


EOF
cat > "$output/assistread074.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r --assist' 'assistread074'

# TEST 75 --- assistread075 --- Search by type (valid type) (no tasks) (mayus)
cat > "$input/assistread075.in" << \EOF








SUPPORT


EOF
cat > "$output/assistread075.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread075'

# TEST 76 --- assistread076 --- Search by type (valid type) (mixed [mayus | minus] )
cat > "$input/assistread076.in" << \EOF








tEsT


EOF
cat > "$output/assistread076.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1   State: NEW   Priority: HIGH   Type: TEST
EOF
./launch-test.sh 'git task -r --assist' 'assistread076'

# TEST 77 --- assistread077 --- Search by type (valid type) (no tasks) (mixed [mayus | minus])
cat > "$input/assistread077.in" << \EOF








suPPorT


EOF
cat > "$output/assistread077.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread077'

# TEST 78 --- assistread078 --- Search by estimated time (invalid data)
cat > "$input/assistread078.in" << \EOF









invalid

EOF
cat > "$output/assistread078.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread078'

# TEST 79 --- assistread079 --- Search by real time (invalid data)
cat > "$input/assistread079.in" << \EOF










invalid
EOF
cat > "$output/assistread079.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread079'

# TEST 80 --- assistread080 --- Search by estimated time (valid data)
cat > "$input/assistread080.in" << \EOF









18

EOF
cat > "$output/assistread080.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
3 | Name: task 3   State: IN PROGRESS   Priority: MAJOR   Type: MANAGEMENT
EOF
./launch-test.sh 'git task -r --assist' 'assistread080'

# TEST 81 --- assistread081 --- Search by real time (valid data)
cat > "$input/assistread081.in" << \EOF










29
EOF
cat > "$output/assistread081.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
4 | Name: task 4   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
7 | Name: same name   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r --assist' 'assistread081'

# TEST 82 --- assistread082 --- Search by estimated time (valid data number) (overflow [more than 10 digits])
cat > "$input/assistread082.in" << \EOF









12345678901

EOF
cat > "$output/assistread082.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread082'

# TEST 83 --- assistread083 --- Search by real time (valid data number) (overflow [more than 10 digits])
cat > "$input/assistread083.in" << \EOF










12345678901
EOF
cat > "$output/assistread083.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread083'

# TEST 84 --- assistread084 --- Search by estimated time (valid data number) (zero)
cat > "$input/assistread084.in" << \EOF









0

EOF
cat > "$output/assistread084.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread084'

# TEST 85 --- assistread085 --- Search by real time (valid data number) (zero)
cat > "$input/assistread085.in" << \EOF










0
EOF
cat > "$output/assistread085.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread085'

# TEST 86 --- assistread086 --- Search by estimated time (valid data number) (negative limit -1)
cat > "$input/assistread086.in" << \EOF









-1

EOF
cat > "$output/assistread086.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread086'

# TEST 87 --- assistread087 --- Search by real time (valid data number) (negative limit -1)
cat > "$input/assistread087.in" << \EOF










-1
EOF
cat > "$output/assistread087.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread087'

# TEST 88 --- assistread088 --- Search by estimated time (valid data number) (negative other)
cat > "$input/assistread088.in" << \EOF









-18

EOF
cat > "$output/assistread088.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread088'

# TEST 89 --- assistread089 --- Search by real time (valid data number) (negative other)
cat > "$input/assistread089.in" << \EOF










-29
EOF
cat > "$output/assistread089.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r --assist' 'assistread089'

# TEST 90 --- assistread090 --- Search by estimated time (valid data number) (no tasks)
cat > "$input/assistread090.in" << \EOF









46

EOF
cat > "$output/assistread090.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread090'

# TEST 91 --- assistread091 --- Search by real time (valid data number) (no tasks)
cat > "$input/assistread091.in" << \EOF










84
EOF
cat > "$output/assistread091.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread091'

# TEST 92 --- assistread092 --- Search task by state (valid state) (no tasks match) (minus)
cat > "$input/assistread092.in" << \EOF


resolved








EOF
cat > "$output/assistread092.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread092'

# TEST 93 --- assistread093 --- Search task by state (valid state) (no tasks match) (mayus)
cat > "$input/assistread093.in" << \EOF


RESOLVED








EOF
cat > "$output/assistread093.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread093'

# TEST 94 --- assistread094 --- Search task by state (valid state) (no tasks match) (mixed [mayus | minus])
cat > "$input/assistread094.in" << \EOF


ResOlVEd








EOF
cat > "$output/assistread094.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread094'

# TODO: fix this test
# TEST 95 --- assistread095 --- Search all tasks (only one in database)
cat > "$input/assistread095.in" << \EOF











EOF
cat > "$output/assistread095.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
EOF
./launch-test.sh 'git task -r --assist' 'assistread095' 'only-one'

# TEST 96 --- assistread096 --- Search tasks by name (multiple tasks with same name)
cat > "$input/assistread096.in" << \EOF

same name









EOF
cat > "$output/assistread096.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
6 | Name: same name   State: IN PROGRESS   Priority: VERY LOW   Type: ANALYSIS
7 | Name: same name   State: REJECTED   Priority: URGENT   Type: DEVELOPMENT
EOF
./launch-test.sh 'git task -r --assist' 'assistread096'


# TEST 97 --- assistread097 --- Search all tasks (empty database)
cat > "$input/assistread097.in" << \EOF











EOF
cat > "$output/assistread097.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread097' 'empty'

# TEST 98 --- assistread098 --- Search task by valid state (empty database) (minus)
cat > "$input/assistread098.in" << \EOF


new








EOF
cat > "$output/assistread098.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread098' 'empty'

# TEST 99 --- assistread099 --- Search task by valid state (empty database) (mayus)
cat > "$input/assistread099.in" << \EOF


NEW








EOF
cat > "$output/assistread099.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread099' 'empty'

# TEST 100 --- assistread100 --- Search task by valid state (empty database) (mixed [mayus | minus])
cat > "$input/assistread100.in" << \EOF


nEW








EOF
cat > "$output/assistread100.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread100' 'empty'

# TEST 101 --- assistread101 --- Search task by valid priority (empty database) (minus)
cat > "$input/assistread101.in" << \EOF







high



EOF
cat > "$output/assistread101.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread101' 'empty'

# TEST 102 --- assistread102 --- Search task by valid priority (empty database) (mayus)
cat > "$input/assistread102.in" << \EOF







HIGH



EOF
cat > "$output/assistread102.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread102' 'empty'

# TEST 103 --- assistread103 --- Search task by valid priority (empty database) (mixed [mayus | minus])
cat > "$input/assistread103.in" << \EOF







hIgH



EOF
cat > "$output/assistread103.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread103' 'empty'

# TEST 104 --- assistread104 --- Search task by valid type (empty database) (mixed [mayus | minus])
cat > "$input/assistread104.in" << \EOF








tEsT


EOF
cat > "$output/assistread104.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread104' 'empty'

# TEST 105 --- assistread105 --- Search task by valid type (empty database) (mayus)
cat > "$input/assistread105.in" << \EOF








TEST


EOF
cat > "$output/assistread105.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread105' 'empty'

# TEST 106 --- assistread106 --- Search task by valid type (empty database) (minus)
cat > "$input/assistread106.in" << \EOF








test


EOF
cat > "$output/assistread106.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r --assist' 'assistread106' 'empty'
