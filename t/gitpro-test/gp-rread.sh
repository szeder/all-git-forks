#!/bin/bash

source constants.sh

###########################
# 	ROLE SEARCH TESTS
###########################
echo "testing: git role -r"

# TEST 1 --- rread001 --- Read valid role
cat > "$input/rread001.in" << \EOF
EOF
cat > "$output/rread001.out" << \EOF
TEST_A can do following actions:
+ create role
+ assign role
EOF
./launch-test.sh 'git role -r -n TEST_A' 'rread001'

# TEST 2 --- rread002 --- Read invalid role
cat > "$input/rread002.in" << \EOF
EOF
cat > "$output/rread002.out" << \EOF
Role you're trying to read doesn't exists
EOF
./launch-test.sh 'git role -r -n TEST_C' 'rread002'

# TEST 3 --- rread003 --- Read valid role without permissions
cat > "$input/rread003.in" << \EOF
EOF
cat > "$output/rread003.out" << \EOF
TEST_B can do following actions:
EOF
./launch-test.sh 'git role -r -n TEST_B' 'rread003'

# TEST 4 --- rread004 --- Read valid role (my role)
cat > "$input/rread004.in" << \EOF
EOF
cat > "$output/rread004.out" << \EOF
EXAMPLE can do following actions:
+ create role
+ remove role
+ update role
+ assign role
+ create task
+ read task
+ update task
+ delete task
+ assign task
+ link files to task
EOF
./launch-test.sh 'git role --myrole' 'rread004'

# TEST 5 --- rread005 --- Read valid role (all roles)
cat > "$input/rread005.in" << \EOF
EOF
cat > "$output/rread005.out" << \EOF
Existent roles:
> TEST_A
> TEST_B
> EXAMPLE
EOF
./launch-test.sh 'git role --show-all' 'rread005'
