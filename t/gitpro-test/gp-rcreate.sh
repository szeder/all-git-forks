#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	ROLE CREATION TESTS
###########################
echo "testing: git task -c"

./clean-db.sh
# TEST  1 --- rcreate001 --- Creates a new successfull role
cat > "$input/rcreate001.in" << \EOF
EOF
cat > "$output/rcreate001.out" << \EOF
Task created successfully
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low' 'rcreate001'
