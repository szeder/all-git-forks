#!/bin/bash

source constants.sh

###########################
# 	TASK CREATION TESTS
###########################
echo "testing: git task -c"

# TEST  1 --- create001 --- Creates a new successfull basic task
cat > "$input/create001.in" << \EOF
EOF
cat > "$output/create001.out" << \EOF
Task created successfully
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low' 'create001'

# TEST  2 --- create002 --- Creates a duplicate task after other before
cat > "$input/create002.in" << \EOF
EOF
cat > "$output/create002.out" << \EOF
Task specified already exists
EOF
./launch-test.sh 'git task -c -n "task 1" -s new -t test -p high' 'create002'

# TEST  3 --- create003 --- Tries to create an task without basic info
cat > "$input/create003.in" << \EOF
EOF
cat > "$output/create003.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test' 'create003'

# TEST  4 --- create004 --- Tries to create an task with invalid state
cat > "$input/create004.in" << \EOF
EOF
cat > "$output/create004.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s invalid_state -t test -p low' 'create004'

# TEST  5 --- create005 --- Tries to create an task with invalid type
cat > "$input/create005.in" << \EOF
EOF
cat > "$output/create005.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t invalid_type -p low' 'create005'

# TEST  6 --- create006 --- Tries to create an task with invalid priority
cat > "$input/create006.in" << \EOF
EOF
cat > "$output/create006.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p invalid_prior' 'create006'

# TEST  7 --- create007 --- Tries to create an task with valid dates
cat > "$input/create007.in" << \EOF
EOF
cat > "$output/create007.out" << \EOF
Task created successfully
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --est_start 03/03/2014 --est_end 05/03/2014 --start 04/03/2014 --end 10/03/2014' 'create007'

# TEST  8 --- create008 --- Tries to create an task with invalid dates (day)
cat > "$input/create008.in" << \EOF
EOF
cat > "$output/create008.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 32/03/2014' 'create008'

# TEST  9 --- create009 --- Tries to create an task with valid times
cat > "$input/create009.in" << \EOF
EOF
cat > "$output/create009.out" << \EOF
Task created successfully
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --est_time 3' 'create009'

# TEST 10 --- create010 --- Tries to create an task with invalid times
cat > "$input/create010.in" << \EOF
EOF
cat > "$output/create010.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --est_time 12345678901' 'create010'

# TEST 11 --- create011 --- Creating a valid task with all possible data fields
cat > "$input/create011.in" << \EOF
EOF
cat > "$output/create011.out" << \EOF
Task created successfully
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --est_start 03/03/2014 --est_end 05/03/2014 --start 04/03/2014 --end 10/03/2014 --est_time 4 --time 2 --notes "my task notes" --desc "task description"' 'create011'

# TEST 12 --- create012 --- Tries to create an task with invalid times (negative)
cat > "$input/create012.in" << \EOF
EOF
cat > "$output/create012.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --est_time -20' 'create012'

# TEST 13 --- create013 --- Tries to create an task with invalid times (zero)
cat > "$input/create013.in" << \EOF
EOF
cat > "$output/create013.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --est_time 0' 'create013'

# TEST 14 --- create014 --- Tries to create an task with specific id
cat > "$input/create014.in" << \EOF
EOF
cat > "$output/create014.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --id 5' 'create014'

# TEST 15 --- create015 --- Tries to create an task with invalid dates (month)
cat > "$input/create015.in" << \EOF
EOF
cat > "$output/create015.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 12/13/2014' 'create015'

# TEST 16 --- create016 --- Tries to create an task with invalid dates (month) (zero)
cat > "$input/create016.in" << \EOF
EOF
cat > "$output/create016.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 12/00/2014' 'create016'

# TEST 17 --- create017 --- Tries to create an task with invalid dates (month) (negative)
cat > "$input/create017.in" << \EOF
EOF
cat > "$output/create017.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 12/-12/2014' 'create017'

# TEST 18 --- create018 --- Tries to create an task with invalid dates (day) (zero)
cat > "$input/create018.in" << \EOF
EOF
cat > "$output/create018.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 00/12/2014' 'create018'

# TEST 19 --- create019 --- Tries to create an task with invalid dates (day) (negative)
cat > "$input/create019.in" << \EOF
EOF
cat > "$output/create019.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start -20/12/2014' 'create019'

# TEST 20 --- create020 --- Tries to create an task with invalid dates (year) (more than 4 numbers)
cat > "$input/create020.in" << \EOF
EOF
cat > "$output/create020.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 05/12/20145' 'create020'

# TEST 21 --- create021 --- Tries to create an task with invalid dates (year) (zero)
cat > "$input/create021.in" << \EOF
EOF
cat > "$output/create021.out" << \EOF
Task created successfully
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 05/12/0000' 'create021'

# TEST 22 --- create022 --- Tries to create an task with invalid dates (year) (negative)
cat > "$input/create022.in" << \EOF
EOF
cat > "$output/create022.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 05/12/-2014' 'create022'

# TEST  23 --- create023 --- Tries to create an task with valid estimated dates but start after end
cat > "$input/create023.in" << \EOF
EOF
cat > "$output/create023.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --est_start 01/08/2014 --est_end 05/03/2014' 'create023'

# TEST  24 --- create024 --- Tries to create an task with valid real dates but start after end
cat > "$input/create024.in" << \EOF
EOF
cat > "$output/create024.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low --start 10/03/2014 --end 05/03/2014' 'create024'
