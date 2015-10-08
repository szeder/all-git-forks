#!/bin/bash

source constants.sh

###########################
# 	ROLE CREATION TESTS
###########################
echo "testing: git role -c"

# TEST  1 --- rcreate001 --- Creates a new successfull role
cat > "$input/rcreate001.in" << \EOF
EOF
cat > "$output/rcreate001.out" << \EOF
Role created successfully
EOF
./launch-test.sh 'git role -c -n rcreate_1 -p 1010101010' 'rcreate001'

# TEST  2 --- rcreate002 --- Creates a duplicated role
cat > "$input/rcreate002.in" << \EOF
EOF
cat > "$output/rcreate002.out" << \EOF
Role name specified already exists
EOF
./launch-test.sh 'git role -c -n TEST_A -p 1010101010' 'rcreate002'

# TEST  3 --- rcreate003 --- Creates a role with incorrect data
cat > "$input/rcreate003.in" << \EOF
EOF
cat > "$output/rcreate003.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -c -n rcreate_1 -p 1010111010101010' 'rcreate003'

# TEST  4 --- rcreate004 --- Creates a role with incorrect data (letters in array bit)
cat > "$input/rcreate004.in" << \EOF
EOF
cat > "$output/rcreate004.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -c -n rcreate_1 -p 1101d01010' 'rcreate004'
