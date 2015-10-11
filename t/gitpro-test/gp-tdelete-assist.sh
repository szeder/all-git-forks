#!/bin/bash

source constants.sh

###########################
# 	TASK assistdelete TESTS
###########################
echo "testing: git task -d --assist"

# TEST 1 --- assistdelete001 --- assistdelete all task (no filters) (multiple tasks exists)
cat > "$input/assistdelete001.in" << \EOF











EOF
cat > "$output/assistdelete001.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
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
./launch-test.sh 'git task -d --assist' 'assistdelete001' 'no-asignations'

# TEST 2 --- assistdelete002 --- assistdelete one task (id filter) (task exists)
cat > "$input/assistdelete002.in" << \EOF
3










EOF
cat > "$output/assistdelete002.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete002' 'no-asignations'

# TEST 3 --- assistdelete003 --- assistdelete task by name (had to match exactly) (one task finded)
cat > "$input/assistdelete003.in" << \EOF

task 5









EOF
cat > "$output/assistdelete003.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete003' 'no-asignations'

# TEST 4 --- assistdelete004 --- assistdelete task by state (multiple task matches) (minus)
cat > "$input/assistdelete004.in" << \EOF


in progress








EOF
cat > "$output/assistdelete004.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete004' 'no-asignations'

# TEST 5 --- assistdelete005 --- assistdelete task by state (multiple task matches) (mayus)
cat > "$input/assistdelete005.in" << \EOF


IN PROGRESS








EOF
cat > "$output/assistdelete005.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete005' 'no-asignations'

# TEST 6 --- assistdelete006 --- assistdelete task by state (multiple task matches) (mayus & minus)
cat > "$input/assistdelete006.in" << \EOF


iN proGrEss








EOF
cat > "$output/assistdelete006.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete006' 'no-asignations'

# TEST 7 --- assistdelete007 --- assistdelete by id (invalid id)
cat > "$input/assistdelete007.in" << \EOF
hola










EOF
cat > "$output/assistdelete007.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete007'

# TEST 8 --- assistdelete008 --- assistdelete by id (** All selected task removed successfully)
cat > "$input/assistdelete008.in" << \EOF
20










EOF
cat > "$output/assistdelete008.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete008'

# TEST 9 --- assistdelete009 --- assistdelete by name (only part of name matches)
cat > "$input/assistdelete009.in" << \EOF

inexistent









EOF
cat > "$output/assistdelete009.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete009'

# TEST 10 --- assistdelete010 --- assistdelete task by name (part of name matching only with one task)
cat > "$input/assistdelete010.in" << \EOF

5









EOF
cat > "$output/assistdelete010.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete010' 'no-asignations'

# TEST 11 --- assistdelete011 --- assistdelete task by name (part of name matching with multiple task names)
cat > "$input/assistdelete011.in" << \EOF

task









EOF
cat > "$output/assistdelete011.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
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
./launch-test.sh 'git task -d --assist' 'assistdelete011' 'no-asignations'

# TEST 12 --- assistdelete012 --- assistdelete task by state (only one matching) (minus)
cat > "$input/assistdelete012.in" << \EOF


new








EOF
cat > "$output/assistdelete012.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete012' 'no-asignations'

# TEST 13 --- assistdelete013 --- assistdelete task by state (only one matching) (mayus)
cat > "$input/assistdelete013.in" << \EOF


NEW








EOF
cat > "$output/assistdelete013.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete013' 'no-asignations'

# TEST 14 --- assistdelete014 --- assistdelete task by state (only one matching) (mixed mayus and minus)
cat > "$input/assistdelete014.in" << \EOF


New








EOF
cat > "$output/assistdelete014.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete014' 'no-asignations'

# TEST 15 --- assistdelete015 --- assistdelete task by state (invalid / inexistent state)
cat > "$input/assistdelete015.in" << \EOF


inexistent








EOF
cat > "$output/assistdelete015.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete015'

# TEST 16 --- assistdelete016 --- assistdelete task by estimated init date (invalid date letters)
cat > "$input/assistdelete016.in" << \EOF



date







EOF
cat > "$output/assistdelete016.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete016'

# TEST 17 --- assistdelete017 --- assistdelete task by estimated init date (invalid day in date)
cat > "$input/assistdelete017.in" << \EOF



50/12/2014







EOF
cat > "$output/assistdelete017.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete017'

# TEST 18 --- assistdelete018 --- assistdelete task by estimated init date (invalid month in date)
cat > "$input/assistdelete018.in" << \EOF



15/16/2014







EOF
cat > "$output/assistdelete018.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete018'

# TEST 19 --- assistdelete019 --- assistdelete task by estimated init date (invalid year in date)
cat > "$input/assistdelete019.in" << \EOF



15/05/23







EOF
cat > "$output/assistdelete019.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete019'

# TEST 20 --- assistdelete020 --- assistdelete task by estimated init date (invalid day in date) (negative)
cat > "$input/assistdelete020.in" << \EOF



-12/06/2014







EOF
cat > "$output/assistdelete020.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete020'

# TEST 21 --- assistdelete021 --- assistdelete task by estimated init date (invalid day in date) (zero)
cat > "$input/assistdelete021.in" << \EOF



00/06/2014







EOF
cat > "$output/assistdelete021.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete021'

# TEST 22 --- assistdelete022 --- assistdelete task by estimated  init date (invalid month in date) (zero)
cat > "$input/assistdelete022.in" << \EOF



15/00/2014







EOF
cat > "$output/assistdelete022.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete022'

# TEST 23 --- assistdelete023 --- assistdelete task by estimated init date (invalid month in date) (negative)
cat > "$input/assistdelete023.in" << \EOF



15/-11/2014







EOF
cat > "$output/assistdelete023.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete023'

# TEST 24 --- assistdelete024 --- assistdelete task by estimated init date (invalid year in date) (over four numbers)
cat > "$input/assistdelete024.in" << \EOF



15/05/20146







EOF
cat > "$output/assistdelete024.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete024'

# TEST 25 --- assistdelete025 --- assistdelete task by estimated init date (invalid year in date) (negative)
cat > "$input/assistdelete025.in" << \EOF



15/11/-2014







EOF
cat > "$output/assistdelete025.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete025'

# TEST 26 --- assistdelete026 --- assistdelete task by estimated final date (invalid date letters)
cat > "$input/assistdelete026.in" << \EOF




date






EOF
cat > "$output/assistdelete026.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete026'

# TEST 27 --- assistdelete027 --- assistdelete task by estimated final date (invalid day in date)
cat > "$input/assistdelete027.in" << \EOF




50/12/2014







EOF
cat > "$output/assistdelete027.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete027'

# TEST 28 --- assistdelete028 --- assistdelete task by estimated final date (invalid month in date)
cat > "$input/assistdelete028.in" << \EOF




15/16/2014







EOF
cat > "$output/assistdelete028.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete028'

# TEST 29 --- assistdelete029 --- assistdelete task by estimated final date (invalid year in date)
cat > "$input/assistdelete029.in" << \EOF




15/05/23







EOF
cat > "$output/assistdelete029.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete029'

# TEST 30 --- assistdelete030 --- assistdelete task by estimated final date (invalid day in date) (negative)
cat > "$input/assistdelete030.in" << \EOF




-12/10/2014






EOF
cat > "$output/assistdelete030.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete030'

# TEST 31 --- assistdelete031 --- assistdelete task by estimated final date (invalid day in date) (zero)
cat > "$input/assistdelete031.in" << \EOF




00/05/2014






EOF
cat > "$output/assistdelete031.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete031'

# TEST 32 --- assistdelete032 --- assistdelete task by estimated final date (invalid month in date) (zero)
cat > "$input/assistdelete032.in" << \EOF




15/00/2014






EOF
cat > "$output/assistdelete032.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete032'

# TEST 33 --- assistdelete033 --- assistdelete task by estimated final date (invalid month in date) (negative)
cat > "$input/assistdelete033.in" << \EOF




15/-11/2014






EOF
cat > "$output/assistdelete033.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete033'

# TEST 34 --- assistdelete034 --- assistdelete task by estimated final date (invalid year in date) (over four numbers)
cat > "$input/assistdelete034.in" << \EOF




15/05/20146






EOF
cat > "$output/assistdelete034.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete034'

# TEST 35 --- assistdelete035 --- assistdelete task by estimated final date (invalid year in date) (negative)
cat > "$input/assistdelete035.in" << \EOF




15/11/-2014






EOF
cat > "$output/assistdelete035.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete035'

# TEST 36 --- assistdelete036 --- assistdelete task by real init date (invalid date letters)
cat > "$input/assistdelete036.in" << \EOF





date





EOF
cat > "$output/assistdelete036.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete036'

# TEST 37 --- assistdelete037 --- assistdelete task by real init date (invalid day in date)
cat > "$input/assistdelete037.in" << \EOF





50/12/2014






EOF
cat > "$output/assistdelete037.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete037'

# TEST 38 --- assistdelete038 --- assistdelete task by real init date (invalid month in date)
cat > "$input/assistdelete038.in" << \EOF





15/16/2014






EOF
cat > "$output/assistdelete038.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete038'

# TEST 39 --- assistdelete039 --- assistdelete task by real init date (invalid year in date)
cat > "$input/assistdelete039.in" << \EOF





15/05/23






EOF
cat > "$output/assistdelete039.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete039'

# TEST 40 --- assistdelete040 --- assistdelete task by real init date (invalid day in date) (negative)
cat > "$input/assistdelete040.in" << \EOF





-12/10/2014





EOF
cat > "$output/assistdelete040.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete040'

# TEST 41 --- assistdelete041 --- assistdelete task by real init date (invalid day in date) (zero)
cat > "$input/assistdelete041.in" << \EOF





00/11/2014





EOF
cat > "$output/assistdelete041.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete041'

# TEST 42 --- assistdelete042 --- assistdelete task by real init date (invalid month in date) (zero)
cat > "$input/assistdelete042.in" << \EOF





15/00/2014





EOF
cat > "$output/assistdelete042.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete042'

# TEST 43 --- assistdelete043 --- assistdelete task by real init date (invalid month in date) (negative)
cat > "$input/assistdelete043.in" << \EOF





15/-11/2014





EOF
cat > "$output/assistdelete043.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete043'

# TEST 44 --- assistdelete044 --- assistdelete task by real init date (invalid year in date) (over four numbers)
cat > "$input/assistdelete044.in" << \EOF





15/05/20146





EOF
cat > "$output/assistdelete044.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete044'

# TEST 45 --- assistdelete045 --- assistdelete task by real init date (invalid year in date) (negative)
cat > "$input/assistdelete045.in" << \EOF





15/11/-2014





EOF
cat > "$output/assistdelete045.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete045'

# TEST 46 --- assistdelete046 --- assistdelete task by real final date (invalid date letters)
cat > "$input/assistdelete046.in" << \EOF






date




EOF
cat > "$output/assistdelete046.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete046'

# TEST 47 --- assistdelete047 --- assistdelete task by real final date (invalid day in date)
cat > "$input/assistdelete047.in" << \EOF






50/12/2014





EOF
cat > "$output/assistdelete047.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete047'

# TEST 48 --- assistdelete048 --- assistdelete task by real final date (invalid month in date)
cat > "$input/assistdelete048.in" << \EOF






15/16/2014





EOF
cat > "$output/assistdelete048.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete048'

# TEST 49 --- assistdelete049 --- assistdelete task by real final date (invalid year in date)
cat > "$input/assistdelete049.in" << \EOF






15/05/23





EOF
cat > "$output/assistdelete049.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete049'

# TEST 50 --- assistdelete050 --- assistdelete task by real final date (invalid day in date) (negative)
cat > "$input/assistdelete050.in" << \EOF






-12/10/2014




EOF
cat > "$output/assistdelete050.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete050'

# TEST 51 --- assistdelete051 --- assistdelete task by real init date (invalid day in date) (zero)
cat > "$input/assistdelete051.in" << \EOF






00/11/2014




EOF
cat > "$output/assistdelete051.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete051'

# TEST 52 --- assistdelete052 --- assistdelete task by real final date (invalid month in date) (zero)
cat > "$input/assistdelete052.in" << \EOF






15/00/2014




EOF
cat > "$output/assistdelete052.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete052'

# TEST 53 --- assistdelete053 --- assistdelete task by real final date (invalid month in date) (negative)
cat > "$input/assistdelete053.in" << \EOF






15/-11/2014




EOF
cat > "$output/assistdelete053.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete053'

# TEST 54 --- assistdelete054 --- assistdelete task by real final date (invalid year in date) (over four numbers)
cat > "$input/assistdelete054.in" << \EOF






15/05/20146




EOF
cat > "$output/assistdelete054.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete054'

# TEST 55 --- assistdelete055 --- assistdelete task by real final date (invalid year in date) (negative)
cat > "$input/assistdelete055.in" << \EOF






15/11/-2014




EOF
cat > "$output/assistdelete055.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete055'

# TEST 56 --- assistdelete056 --- assistdelete by estimated init date (valid) (one task matching)
cat > "$input/assistdelete056.in" << \EOF



30/12/2014







EOF
cat > "$output/assistdelete056.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete056' 'no-asignations'

# TEST 57 --- assistdelete057 --- assistdelete by estimated final date (valid) (one task matching)
cat > "$input/assistdelete057.in" << \EOF




24/12/2014






EOF
cat > "$output/assistdelete057.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete057'

# TEST 58 --- assistdelete058 --- assistdelete by real init date (valid) (one task matching)
cat > "$input/assistdelete058.in" << \EOF





21/12/2014





EOF
cat > "$output/assistdelete058.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete058' 

# TEST 59 --- assistdelete059 --- assistdelete by real final date (valid) (one task matching)
cat > "$input/assistdelete059.in" << \EOF






28/12/2014




EOF
cat > "$output/assistdelete059.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete059' 'no-asignations'

# TEST 60 --- assistdelete060 --- assistdelete by estimated init date (valid) (** All selected task removed successfully)
cat > "$input/assistdelete060.in" << \EOF



05/05/2014







EOF
cat > "$output/assistdelete060.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete060'

# TEST 61 --- assistdelete061 --- assistdelete by estimated final date (valid) (** All selected task removed successfully)
cat > "$input/assistdelete061.in" << \EOF




05/06/2014






EOF
cat > "$output/assistdelete061.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete061'

# TEST 62 --- assistdelete062 --- assistdelete by real init date (valid) (** All selected task removed successfully)
cat > "$input/assistdelete062.in" << \EOF





05/08/2014





EOF
cat > "$output/assistdelete062.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete062'

# TEST 63 --- assistdelete063 --- assistdelete by real final date (valid) (** All selected task removed successfully)
cat > "$input/assistdelete063.in" << \EOF






28/02/2014




EOF
cat > "$output/assistdelete063.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete063'

# TEST 64 --- assistdelete064 --- assistdelete by priority (inexistent prior)
cat > "$input/assistdelete064.in" << \EOF







inexistent



EOF
cat > "$output/assistdelete064.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete064'

# TEST 65 --- assistdelete065 --- assistdelete by priority (valid prior) (minus)
cat > "$input/assistdelete065.in" << \EOF







very high



EOF
cat > "$output/assistdelete065.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete065' 'no-asignations'

# TEST 66 --- assistdelete066 --- assistdelete by priority (valid prior) (no tasks) (minus)
cat > "$input/assistdelete066.in" << \EOF







blocker



EOF
cat > "$output/assistdelete066.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete066'

# TEST 67 --- assistdelete067 --- assistdelete by priority (valid prior) (mayus)
cat > "$input/assistdelete067.in" << \EOF







VERY HIGH



EOF
cat > "$output/assistdelete067.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete067' 'no-asignations'

# TEST 68 --- assistdelete068 --- assistdelete by priority (valid prior) (no tasks) (mayus)
cat > "$input/assistdelete068.in" << \EOF







BLOCKER



EOF
cat > "$output/assistdelete068.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete068'

# TEST 69 --- assistdelete069 --- assistdelete by priority (valid prior) (mixed [mayus | minus] )
cat > "$input/assistdelete069.in" << \EOF







veRy hIgH



EOF
cat > "$output/assistdelete069.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
- Removing asignations and asociations to task 8 ...
- Task 8 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete069' 'no-asignations'

# TEST 70 --- assistdelete070 --- assistdelete by priority (valid prior) (no tasks) (mixed [mayus | minus])
cat > "$input/assistdelete070.in" << \EOF







blOcKEr



EOF
cat > "$output/assistdelete070.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete070'

# TEST 71 --- assistdelete071 --- assistdelete by type (inexistent type)
cat > "$input/assistdelete071.in" << \EOF








inexistent


EOF
cat > "$output/assistdelete071.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete071'

# TEST 72 --- assistdelete072 --- assistdelete by type (valid type) (minus)
cat > "$input/assistdelete072.in" << \EOF








test


EOF
cat > "$output/assistdelete072.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete072' 'no-asignations'

# TEST 73 --- assistdelete073 --- assistdelete by type (valid type) (no tasks) (minus)
cat > "$input/assistdelete073.in" << \EOF








support


EOF
cat > "$output/assistdelete073.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete073'

# TEST 74 --- assistdelete074 --- assistdelete by type (valid type) (mayus)
cat > "$input/assistdelete074.in" << \EOF








TEST


EOF
cat > "$output/assistdelete074.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete074' 'no-asignations'

# TEST 75 --- assistdelete075 --- assistdelete by type (valid type) (no tasks) (mayus)
cat > "$input/assistdelete075.in" << \EOF








SUPPORT


EOF
cat > "$output/assistdelete075.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete075'

# TEST 76 --- assistdelete076 --- assistdelete by type (valid type) (mixed [mayus | minus] )
cat > "$input/assistdelete076.in" << \EOF








tEsT


EOF
cat > "$output/assistdelete076.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete076' 'no-asignations'

# TEST 77 --- assistdelete077 --- assistdelete by type (valid type) (no tasks) (mixed [mayus | minus])
cat > "$input/assistdelete077.in" << \EOF








suPPorT


EOF
cat > "$output/assistdelete077.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete077'

# TEST 78 --- assistdelete078 --- assistdelete by estimated time (invalid data)
cat > "$input/assistdelete078.in" << \EOF









invalid

EOF
cat > "$output/assistdelete078.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete078'

# TEST 79 --- assistdelete079 --- assistdelete by real time (invalid data)
cat > "$input/assistdelete079.in" << \EOF










invalid
EOF
cat > "$output/assistdelete079.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete079'

# TEST 80 --- assistdelete080 --- assistdelete by estimated time (valid data)
cat > "$input/assistdelete080.in" << \EOF









18

EOF
cat > "$output/assistdelete080.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete080' 'no-asignations'

# TEST 81 --- assistdelete081 --- assistdelete by real time (valid data)
cat > "$input/assistdelete081.in" << \EOF










29
EOF
cat > "$output/assistdelete081.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 4 ...
- Task 4 prepared to be removed
- Removing asignations and asociations to task 7 ...
- Task 7 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete081'

# TEST 82 --- assistdelete082 --- assistdelete by estimated time (valid data number) (overflow [more than 10 digits])
cat > "$input/assistdelete082.in" << \EOF









12345678901

EOF
cat > "$output/assistdelete082.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete082'

# TEST 83 --- assistdelete083 --- assistdelete by real time (valid data number) (overflow [more than 10 digits])
cat > "$input/assistdelete083.in" << \EOF










12345678901
EOF
cat > "$output/assistdelete083.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete083'

# TEST 84 --- assistdelete084 --- assistdelete by estimated time (valid data number) (zero)
cat > "$input/assistdelete084.in" << \EOF









0

EOF
cat > "$output/assistdelete084.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete084'

# TEST 85 --- assistdelete085 --- assistdelete by real time (valid data number) (zero)
cat > "$input/assistdelete085.in" << \EOF










0
EOF
cat > "$output/assistdelete085.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete085'

# TEST 86 --- assistdelete086 --- assistdelete by estimated time (valid data number) (negative limit -1)
cat > "$input/assistdelete086.in" << \EOF









-1

EOF
cat > "$output/assistdelete086.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete086'

# TEST 87 --- assistdelete087 --- assistdelete by real time (valid data number) (negative limit -1)
cat > "$input/assistdelete087.in" << \EOF










-1
EOF
cat > "$output/assistdelete087.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete087'

# TEST 88 --- assistdelete088 --- assistdelete by estimated time (valid data number) (negative other)
cat > "$input/assistdelete088.in" << \EOF









-18

EOF
cat > "$output/assistdelete088.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete088'

# TEST 89 --- assistdelete089 --- assistdelete by real time (valid data number) (negative other)
cat > "$input/assistdelete089.in" << \EOF










-29
EOF
cat > "$output/assistdelete089.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete089'

# TEST 90 --- assistdelete090 --- assistdelete by estimated time (valid data number) (no tasks)
cat > "$input/assistdelete090.in" << \EOF









46

EOF
cat > "$output/assistdelete090.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete090'

# TEST 91 --- assistdelete091 --- assistdelete by real time (valid data number) (no tasks)
cat > "$input/assistdelete091.in" << \EOF










84
EOF
cat > "$output/assistdelete091.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete091'

# TEST 92 --- assistdelete092 --- assistdelete task by state (valid state) (no tasks match) (minus)
cat > "$input/assistdelete092.in" << \EOF


resolved








EOF
cat > "$output/assistdelete092.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete092'

# TEST 93 --- assistdelete093 --- assistdelete task by state (valid state) (no tasks match) (mayus)
cat > "$input/assistdelete093.in" << \EOF


RESOLVED








EOF
cat > "$output/assistdelete093.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete093'

# TEST 94 --- assistdelete094 --- assistdelete task by state (valid state) (no tasks match) (mixed [mayus | minus])
cat > "$input/assistdelete094.in" << \EOF


ResOlVEd








EOF
cat > "$output/assistdelete094.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete094'

# TEST 95 --- assistdelete095 --- assistdelete all tasks (only one in database)
cat > "$input/assistdelete095.in" << \EOF











EOF
cat > "$output/assistdelete095.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete095' 'only-one'

# TEST 96 --- assistdelete096 --- assistdelete tasks by name (multiple tasks with same name)
cat > "$input/assistdelete096.in" << \EOF

same name









EOF
cat > "$output/assistdelete096.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 6 ...
- Task 6 prepared to be removed
- Removing asignations and asociations to task 7 ...
- Task 7 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete096'

# TEST 97 --- assistdelete097 --- assistdelete all tasks (empty database)
cat > "$input/assistdelete097.in" << \EOF











EOF
cat > "$output/assistdelete097.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete097' 'empty'

# TEST 98 --- assistdelete098 --- assistdelete task by valid state (empty database) (minus)
cat > "$input/assistdelete098.in" << \EOF


new








EOF
cat > "$output/assistdelete098.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete098' 'empty'

# TEST 99 --- assistdelete099 --- assistdelete task by valid state (empty database) (mayus)
cat > "$input/assistdelete099.in" << \EOF


NEW








EOF
cat > "$output/assistdelete099.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete099' 'empty'

# TEST 100 --- assistdelete100 --- assistdelete task by valid state (empty database) (mixed [mayus | minus])
cat > "$input/assistdelete100.in" << \EOF


nEW








EOF
cat > "$output/assistdelete100.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete100' 'empty'

# TEST 101 --- assistdelete101 --- assistdelete task by valid priority (empty database) (minus)
cat > "$input/assistdelete101.in" << \EOF







high



EOF
cat > "$output/assistdelete101.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete101' 'empty'

# TEST 102 --- assistdelete102 --- assistdelete task by valid priority (empty database) (mayus)
cat > "$input/assistdelete102.in" << \EOF







HIGH



EOF
cat > "$output/assistdelete102.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete102' 'empty'

# TEST 103 --- assistdelete103 --- assistdelete task by valid priority (empty database) (mixed [mayus | minus])
cat > "$input/assistdelete103.in" << \EOF







hIgH



EOF
cat > "$output/assistdelete103.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete103' 'empty'

# TEST 104 --- assistdelete104 --- assistdelete task by valid type (empty database) (mixed [mayus | minus])
cat > "$input/assistdelete104.in" << \EOF








tEsT


EOF
cat > "$output/assistdelete104.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete104' 'empty'

# TEST 105 --- assistdelete105 --- assistdelete task by valid type (empty database) (mayus)
cat > "$input/assistdelete105.in" << \EOF








TEST


EOF
cat > "$output/assistdelete105.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete105' 'empty'

# TEST 106 --- assistdelete106 --- assistdelete task by valid type (empty database) (minus)
cat > "$input/assistdelete106.in" << \EOF








test


EOF
cat > "$output/assistdelete106.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: ** All selected task removed successfully
EOF
./launch-test.sh 'git task -d --assist' 'assistdelete106' 'empty'

# SOME TESTS WITH ASOCIATIONS AND ASIGNATIONS IN DATABASE

# TEST 107 --- assistdelete107 --- assistdelete all task (no filters) (multiple tasks exists)
cat > "$input/assistdelete107.in" << \EOF











0
EOF
cat > "$output/assistdelete107.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
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
./launch-test.sh 'git task -d --assist' 'assistdelete107' 'test-asociations'
