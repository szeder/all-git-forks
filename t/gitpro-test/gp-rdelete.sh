#!/bin/bash

source constants.sh

###########################
# 	ROLE Delete TESTS
###########################
echo "testing: git role -d"


# TEST 1 --- rdelete001 --- Delete a valid role
cat > "$input/rdelete001.in" << \EOF
EOF
cat > "$output/rdelete001.out" << \EOF
Role deleted successfully
EOF
./launch-test.sh 'git role -d -n TEST_A' 'rdelete001'

# TEST 2 --- rdelete002 --- Delete a invalid role
cat > "$input/rdelete002.in" << \EOF
EOF
cat > "$output/rdelete002.out" << \EOF
Role you're trying to delete doesn't exists
EOF
./launch-test.sh 'git role -d -n INVALID' 'rdelete002'

# TEST 3 --- rdelete003 --- Delete a invalid role (incorrect data)
cat > "$input/rdelete003.in" << \EOF
EOF
cat > "$output/rdelete003.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -d' 'rdelete003'
