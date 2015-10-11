#!/bin/bash

source constants.sh

###########################
# 	TASK Update TESTS
###########################
echo "testing: git task -u"

# TEST 1 --- update001 --- Update all task (no filters) (multiple tasks exists)
cat > "$input/update001.in" << \EOF











EOF
cat > "$output/update001.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo -n nuevo' 'update001'


# TEST 2 --- update002 --- Update one task (id filter) (task exists)
cat > "$input/update002.in" << \EOF
3










EOF
cat > "$output/update002.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo -n nuevo' 'update002'

# TEST 3 --- update003 --- Update task by name (had to match exactly) (one task finded)
cat > "$input/update003.in" << \EOF

task 5









EOF
cat > "$output/update003.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo -n nuevo' 'update003'

# TEST 4 --- update004 --- Update task by state (multiple task matches) (minus)
cat > "$input/update004.in" << \EOF


in progress








EOF
cat > "$output/update004.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo -n nuevo' 'update004'

# TEST 5 --- update005 --- Update task by state (multiple task matches) (mayus)
cat > "$input/update005.in" << \EOF


IN PROGRESS








EOF
cat > "$output/update005.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update005'

# TEST 6 --- update006 --- Update task by state (multiple task matches) (mayus & minus)
cat > "$input/update006.in" << \EOF


iN proGrEss








EOF
cat > "$output/update006.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update006'

# TEST 7 --- update007 --- Update by id (invalid id)
cat > "$input/update007.in" << \EOF
hola










EOF
cat > "$output/update007.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update007'

# TEST 8 --- update008 --- Update by id (+ Tasks updated successfully)
cat > "$input/update008.in" << \EOF
20










EOF
cat > "$output/update008.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update008'

# TEST 9 --- update009 --- Update by name (only part of name matches)
cat > "$input/update009.in" << \EOF

inexistent









EOF
cat > "$output/update009.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update009'

# TEST 10 --- update010 --- Update task by name (part of name matching only with one task)
cat > "$input/update010.in" << \EOF

5









EOF
cat > "$output/update010.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update010'

# TEST 11 --- update011 --- Update task by name (part of name matching with multiple task names)
cat > "$input/update011.in" << \EOF

task









EOF
cat > "$output/update011.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update011'

# TEST 12 --- update012 --- Update task by state (only one matching) (minus)
cat > "$input/update012.in" << \EOF


new








EOF
cat > "$output/update012.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update012'

# TEST 13 --- update013 --- Update task by state (only one matching) (mayus)
cat > "$input/update013.in" << \EOF


NEW








EOF
cat > "$output/update013.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update013'

# TEST 14 --- update014 --- Update task by state (only one matching) (mixed mayus and minus)
cat > "$input/update014.in" << \EOF


New








EOF
cat > "$output/update014.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update014'

# TEST 15 --- update015 --- Update task by state (invalid / inexistent state)
cat > "$input/update015.in" << \EOF


inexistent








EOF
cat > "$output/update015.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update015'

# TEST 16 --- update016 --- Update task by estimated init date (invalid date letters)
cat > "$input/update016.in" << \EOF



date







EOF
cat > "$output/update016.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update016'

# TEST 17 --- update017 --- Update task by estimated init date (invalid day in date)
cat > "$input/update017.in" << \EOF



50/12/2014







EOF
cat > "$output/update017.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update017'

# TEST 18 --- update018 --- Update task by estimated init date (invalid month in date)
cat > "$input/update018.in" << \EOF



15/16/2014







EOF
cat > "$output/update018.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update018'

# TEST 19 --- update019 --- Update task by estimated init date (invalid year in date)
cat > "$input/update019.in" << \EOF



15/05/23







EOF
cat > "$output/update019.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update019'

# TEST 20 --- update020 --- Update task by estimated init date (invalid day in date) (negative)
cat > "$input/update020.in" << \EOF



-12/06/2014







EOF
cat > "$output/update020.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update020'

# TEST 21 --- update021 --- Update task by estimated init date (invalid day in date) (zero)
cat > "$input/update021.in" << \EOF



00/06/2014







EOF
cat > "$output/update021.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update021'

# TEST 22 --- update022 --- Update task by estimated  init date (invalid month in date) (zero)
cat > "$input/update022.in" << \EOF



15/00/2014







EOF
cat > "$output/update022.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update022'

# TEST 23 --- update023 --- Update task by estimated init date (invalid month in date) (negative)
cat > "$input/update023.in" << \EOF



15/-11/2014







EOF
cat > "$output/update023.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update023'

# TEST 24 --- update024 --- Update task by estimated init date (invalid year in date) (over four numbers)
cat > "$input/update024.in" << \EOF



15/05/20146







EOF
cat > "$output/update024.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update024'

# TEST 25 --- update025 --- Update task by estimated init date (invalid year in date) (negative)
cat > "$input/update025.in" << \EOF



15/11/-2014







EOF
cat > "$output/update025.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update025'

# TEST 26 --- update026 --- Update task by estimated final date (invalid date letters)
cat > "$input/update026.in" << \EOF




date






EOF
cat > "$output/update026.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update026'

# TEST 27 --- update027 --- Update task by estimated final date (invalid day in date)
cat > "$input/update027.in" << \EOF




50/12/2014







EOF
cat > "$output/update027.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update027'

# TEST 28 --- update028 --- Update task by estimated final date (invalid month in date)
cat > "$input/update028.in" << \EOF




15/16/2014







EOF
cat > "$output/update028.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update028'

# TEST 29 --- update029 --- Update task by estimated final date (invalid year in date)
cat > "$input/update029.in" << \EOF




15/05/23







EOF
cat > "$output/update029.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update029'

# TEST 30 --- update030 --- Update task by estimated final date (invalid day in date) (negative)
cat > "$input/update030.in" << \EOF




-12/10/2014






EOF
cat > "$output/update030.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update030'

# TEST 31 --- update031 --- Update task by estimated final date (invalid day in date) (zero)
cat > "$input/update031.in" << \EOF




00/05/2014






EOF
cat > "$output/update031.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update031'

# TEST 32 --- update032 --- Update task by estimated final date (invalid month in date) (zero)
cat > "$input/update032.in" << \EOF




15/00/2014






EOF
cat > "$output/update032.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update032'

# TEST 33 --- update033 --- Update task by estimated final date (invalid month in date) (negative)
cat > "$input/update033.in" << \EOF




15/-11/2014






EOF
cat > "$output/update033.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update033'

# TEST 34 --- update034 --- Update task by estimated final date (invalid year in date) (over four numbers)
cat > "$input/update034.in" << \EOF




15/05/20146






EOF
cat > "$output/update034.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update034'

# TEST 35 --- update035 --- Update task by estimated final date (invalid year in date) (negative)
cat > "$input/update035.in" << \EOF




15/11/-2014






EOF
cat > "$output/update035.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update035'

# TEST 36 --- update036 --- Update task by real init date (invalid date letters)
cat > "$input/update036.in" << \EOF





date





EOF
cat > "$output/update036.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update036'

# TEST 37 --- update037 --- Update task by real init date (invalid day in date)
cat > "$input/update037.in" << \EOF





50/12/2014






EOF
cat > "$output/update037.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update037'

# TEST 38 --- update038 --- Update task by real init date (invalid month in date)
cat > "$input/update038.in" << \EOF





15/16/2014






EOF
cat > "$output/update038.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update038'

# TEST 39 --- update039 --- Update task by real init date (invalid year in date)
cat > "$input/update039.in" << \EOF





15/05/23






EOF
cat > "$output/update039.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update039'

# TEST 40 --- update040 --- Update task by real init date (invalid day in date) (negative)
cat > "$input/update040.in" << \EOF





-12/10/2014





EOF
cat > "$output/update040.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update040'

# TEST 41 --- update041 --- Update task by real init date (invalid day in date) (zero)
cat > "$input/update041.in" << \EOF





00/11/2014





EOF
cat > "$output/update041.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update041'

# TEST 42 --- update042 --- Update task by real init date (invalid month in date) (zero)
cat > "$input/update042.in" << \EOF





15/00/2014





EOF
cat > "$output/update042.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update042'

# TEST 43 --- update043 --- Update task by real init date (invalid month in date) (negative)
cat > "$input/update043.in" << \EOF





15/-11/2014





EOF
cat > "$output/update043.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update043'

# TEST 44 --- update044 --- Update task by real init date (invalid year in date) (over four numbers)
cat > "$input/update044.in" << \EOF





15/05/20146





EOF
cat > "$output/update044.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update044'

# TEST 45 --- update045 --- Update task by real init date (invalid year in date) (negative)
cat > "$input/update045.in" << \EOF





15/11/-2014





EOF
cat > "$output/update045.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update045'

# TEST 46 --- update046 --- Update task by real final date (invalid date letters)
cat > "$input/update046.in" << \EOF






date




EOF
cat > "$output/update046.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update046'

# TEST 47 --- update047 --- Update task by real final date (invalid day in date)
cat > "$input/update047.in" << \EOF






50/12/2014





EOF
cat > "$output/update047.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update047'

# TEST 48 --- update048 --- Update task by real final date (invalid month in date)
cat > "$input/update048.in" << \EOF






15/16/2014





EOF
cat > "$output/update048.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update048'

# TEST 49 --- update049 --- Update task by real final date (invalid year in date)
cat > "$input/update049.in" << \EOF






15/05/23





EOF
cat > "$output/update049.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update049'

# TEST 50 --- update050 --- Update task by real final date (invalid day in date) (negative)
cat > "$input/update050.in" << \EOF






-12/10/2014




EOF
cat > "$output/update050.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update050'

# TEST 51 --- update051 --- Update task by real init date (invalid day in date) (zero)
cat > "$input/update051.in" << \EOF






00/11/2014




EOF
cat > "$output/update051.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update051'

# TEST 52 --- update052 --- Update task by real final date (invalid month in date) (zero)
cat > "$input/update052.in" << \EOF






15/00/2014




EOF
cat > "$output/update052.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update052'

# TEST 53 --- update053 --- Update task by real final date (invalid month in date) (negative)
cat > "$input/update053.in" << \EOF






15/-11/2014




EOF
cat > "$output/update053.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update053'

# TEST 54 --- update054 --- Update task by real final date (invalid year in date) (over four numbers)
cat > "$input/update054.in" << \EOF






15/05/20146




EOF
cat > "$output/update054.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update054'

# TEST 55 --- update055 --- Update task by real final date (invalid year in date) (negative)
cat > "$input/update055.in" << \EOF






15/11/-2014




EOF
cat > "$output/update055.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update055'

# TEST 56 --- update056 --- Update by estimated init date (valid) (one task matching)
cat > "$input/update056.in" << \EOF



30/12/2014







EOF
cat > "$output/update056.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update056'

# TEST 57 --- update057 --- Update by estimated final date (valid) (one task matching)
cat > "$input/update057.in" << \EOF




24/12/2014






EOF
cat > "$output/update057.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update057'

# TEST 58 --- update058 --- Update by real init date (valid) (one task matching)
cat > "$input/update058.in" << \EOF





21/12/2014





EOF
cat > "$output/update058.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update058'

# TEST 59 --- update059 --- Update by real final date (valid) (one task matching)
cat > "$input/update059.in" << \EOF






28/12/2014




EOF
cat > "$output/update059.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update059'

# TEST 60 --- update060 --- Update by estimated init date (valid) (+ Tasks updated successfully)
cat > "$input/update060.in" << \EOF



05/05/2014







EOF
cat > "$output/update060.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update060'

# TEST 61 --- update061 --- Update by estimated final date (valid) (+ Tasks updated successfully)
cat > "$input/update061.in" << \EOF




05/06/2014






EOF
cat > "$output/update061.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update061'

# TEST 62 --- update062 --- Update by real init date (valid) (+ Tasks updated successfully)
cat > "$input/update062.in" << \EOF





05/08/2014





EOF
cat > "$output/update062.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update062'

# TEST 63 --- update063 --- Update by real final date (valid) (+ Tasks updated successfully)
cat > "$input/update063.in" << \EOF






28/02/2014




EOF
cat > "$output/update063.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update063'

# TEST 64 --- update064 --- Update by priority (inexistent prior)
cat > "$input/update064.in" << \EOF







inexistent



EOF
cat > "$output/update064.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update064'

# TEST 65 --- update065 --- Update by priority (valid prior) (minus)
cat > "$input/update065.in" << \EOF







very high



EOF
cat > "$output/update065.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update065'

# TEST 66 --- update066 --- Update by priority (valid prior) (no tasks) (minus)
cat > "$input/update066.in" << \EOF







blocker



EOF
cat > "$output/update066.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update066'

# TEST 67 --- update067 --- Update by priority (valid prior) (mayus)
cat > "$input/update067.in" << \EOF







VERY HIGH



EOF
cat > "$output/update067.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update067'

# TEST 68 --- update068 --- Update by priority (valid prior) (no tasks) (mayus)
cat > "$input/update068.in" << \EOF







BLOCKER



EOF
cat > "$output/update068.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update068'

# TEST 69 --- update069 --- Update by priority (valid prior) (mixed [mayus | minus] )
cat > "$input/update069.in" << \EOF







veRy hIgH



EOF
cat > "$output/update069.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update069'

# TEST 70 --- update070 --- Update by priority (valid prior) (no tasks) (mixed [mayus | minus])
cat > "$input/update070.in" << \EOF







blOcKEr



EOF
cat > "$output/update070.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update070'

# TEST 71 --- update071 --- Update by type (inexistent type)
cat > "$input/update071.in" << \EOF








inexistent


EOF
cat > "$output/update071.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update071'

# TEST 72 --- update072 --- Update by type (valid type) (minus)
cat > "$input/update072.in" << \EOF








test


EOF
cat > "$output/update072.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update072'

# TEST 73 --- update073 --- Update by type (valid type) (no tasks) (minus)
cat > "$input/update073.in" << \EOF








support


EOF
cat > "$output/update073.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update073'

# TEST 74 --- update074 --- Update by type (valid type) (mayus)
cat > "$input/update074.in" << \EOF








TEST


EOF
cat > "$output/update074.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update074'

# TEST 75 --- update075 --- Update by type (valid type) (no tasks) (mayus)
cat > "$input/update075.in" << \EOF








SUPPORT


EOF
cat > "$output/update075.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update075'

# TEST 76 --- update076 --- Update by type (valid type) (mixed [mayus | minus] )
cat > "$input/update076.in" << \EOF








tEsT


EOF
cat > "$output/update076.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update076'

# TEST 77 --- update077 --- Update by type (valid type) (no tasks) (mixed [mayus | minus])
cat > "$input/update077.in" << \EOF








suPPorT


EOF
cat > "$output/update077.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update077'

# TEST 78 --- update078 --- Update by estimated time (invalid data)
cat > "$input/update078.in" << \EOF









invalid

EOF
cat > "$output/update078.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update078'

# TEST 79 --- update079 --- Update by real time (invalid data)
cat > "$input/update079.in" << \EOF










invalid
EOF
cat > "$output/update079.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update079'

# TEST 80 --- update080 --- Update by estimated time (valid data)
cat > "$input/update080.in" << \EOF









18

EOF
cat > "$output/update080.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update080'

# TEST 81 --- update081 --- Update by real time (valid data)
cat > "$input/update081.in" << \EOF










29
EOF
cat > "$output/update081.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update081'

# TEST 82 --- update082 --- Update by estimated time (valid data number) (overflow [more than 10 digits])
cat > "$input/update082.in" << \EOF









12345678901

EOF
cat > "$output/update082.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update082'

# TEST 83 --- update083 --- Update by real time (valid data number) (overflow [more than 10 digits])
cat > "$input/update083.in" << \EOF










12345678901
EOF
cat > "$output/update083.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update083'

# TEST 84 --- update084 --- Update by estimated time (valid data number) (zero)
cat > "$input/update084.in" << \EOF









0

EOF
cat > "$output/update084.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update084'

# TEST 85 --- update085 --- Update by real time (valid data number) (zero)
cat > "$input/update085.in" << \EOF










0
EOF
cat > "$output/update085.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update085'

# TEST 86 --- update086 --- Update by estimated time (valid data number) (negative limit -1)
cat > "$input/update086.in" << \EOF









-1

EOF
cat > "$output/update086.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update086'

# TEST 87 --- update087 --- Update by real time (valid data number) (negative limit -1)
cat > "$input/update087.in" << \EOF










-1
EOF
cat > "$output/update087.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update087'

# TEST 88 --- update088 --- Update by estimated time (valid data number) (negative other)
cat > "$input/update088.in" << \EOF









-18

EOF
cat > "$output/update088.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update088'

# TEST 89 --- update089 --- Update by real time (valid data number) (negative other)
cat > "$input/update089.in" << \EOF










-29
EOF
cat > "$output/update089.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -u -n nuevo' 'update089'

# TEST 90 --- update090 --- Update by estimated time (valid data number) (no tasks)
cat > "$input/update090.in" << \EOF









46

EOF
cat > "$output/update090.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update090'

# TEST 91 --- update091 --- Update by real time (valid data number) (no tasks)
cat > "$input/update091.in" << \EOF










84
EOF
cat > "$output/update091.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update091'

# TEST 92 --- update092 --- Update task by state (valid state) (no tasks match) (minus)
cat > "$input/update092.in" << \EOF


resolved








EOF
cat > "$output/update092.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update092'

# TEST 93 --- update093 --- Update task by state (valid state) (no tasks match) (mayus)
cat > "$input/update093.in" << \EOF


RESOLVED








EOF
cat > "$output/update093.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update093'

# TEST 94 --- update094 --- Update task by state (valid state) (no tasks match) (mixed [mayus | minus])
cat > "$input/update094.in" << \EOF


ResOlVEd








EOF
cat > "$output/update094.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update094'

# TODO: Fix this test
# TEST 95 --- update095 --- Update all tasks (only one in database)
cat > "$input/update095.in" << \EOF











EOF
cat > "$output/update095.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update095'


# TEST 96 --- update096 --- Update tasks by name (multiple tasks with same name)
cat > "$input/update096.in" << \EOF

same name









EOF
cat > "$output/update096.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update096'


# TEST 97 --- update097 --- Update all tasks (empty database)
cat > "$input/update097.in" << \EOF











EOF
cat > "$output/update097.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update097'

# TEST 98 --- update098 --- Update task by valid state (empty database) (minus)
cat > "$input/update098.in" << \EOF


new








EOF
cat > "$output/update098.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update098'

# TEST 99 --- update099 --- Update task by valid state (empty database) (mayus)
cat > "$input/update099.in" << \EOF


NEW








EOF
cat > "$output/update099.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update099'

# TEST 100 --- update100 --- Update task by valid state (empty database) (mixed [mayus | minus])
cat > "$input/update100.in" << \EOF


nEW








EOF
cat > "$output/update100.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update100'

# TEST 101 --- update101 --- Update task by valid priority (empty database) (minus)
cat > "$input/update101.in" << \EOF







high



EOF
cat > "$output/update101.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update101'

# TEST 102 --- update102 --- Update task by valid priority (empty database) (mayus)
cat > "$input/update102.in" << \EOF







HIGH



EOF
cat > "$output/update102.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update102'

# TEST 103 --- update103 --- Update task by valid priority (empty database) (mixed [mayus | minus])
cat > "$input/update103.in" << \EOF







hIgH



EOF
cat > "$output/update103.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update103'

# TEST 104 --- update104 --- Update task by valid type (empty database) (mixed [mayus | minus])
cat > "$input/update104.in" << \EOF








tEsT


EOF
cat > "$output/update104.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update104'

# TEST 105 --- update105 --- Update task by valid type (empty database) (mayus)
cat > "$input/update105.in" << \EOF








TEST


EOF
cat > "$output/update105.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update105'

# TEST 106 --- update106 --- Update task by valid type (empty database) (minus)
cat > "$input/update106.in" << \EOF








test


EOF
cat > "$output/update106.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -n nuevo' 'update106'

# TEST 107 --- update107 --- Update task by state
cat > "$input/update107.in" << \EOF











EOF
cat > "$output/update107.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Invalid state transition
EOF
./launch-test.sh 'git task -u -s new' 'update107'

# TEST 108 --- update108 --- Update task by desc
cat > "$input/update108.in" << \EOF











EOF
cat > "$output/update108.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --desc descrip' 'update108'

# TEST 109 --- update109 --- Update task by notes
cat > "$input/update109.in" << \EOF











EOF
cat > "$output/update109.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --notes mynotes' 'update109'

# TEST 110 --- update110 --- Update task by est start
cat > "$input/update110.in" << \EOF











EOF
cat > "$output/update110.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --est_start 10/10/2015' 'update110'

# TEST 111 --- update111 --- Update task by est end
cat > "$input/update111.in" << \EOF











EOF
cat > "$output/update111.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --est_end 10/10/2015' 'update111'

# TEST 112 --- update112 --- Update task by start
cat > "$input/update112.in" << \EOF











EOF
cat > "$output/update112.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --start 10/10/2015' 'update112'

# TEST 113 --- update113 --- Update task by end
cat > "$input/update113.in" << \EOF











EOF
cat > "$output/update113.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --end 10/10/2015' 'update113'

# TEST 114 --- update114 --- Update task by priority
cat > "$input/update114.in" << \EOF











EOF
cat > "$output/update114.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -p high' 'update114'

# TEST 115 --- update115 --- Update task by type
cat > "$input/update115.in" << \EOF











EOF
cat > "$output/update115.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u -t test' 'update115'

# TEST 116 --- update116 --- Update task by est time
cat > "$input/update116.in" << \EOF











EOF
cat > "$output/update116.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --est_time 4' 'update116'

# TEST 117 --- update117 --- Update task by time
cat > "$input/update117.in" << \EOF











EOF
cat > "$output/update117.out" << \EOF
All filters are by equality
task id: task name (contained text): task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: + Tasks updated successfully
EOF
./launch-test.sh 'git task -u --time 15' 'update117'
