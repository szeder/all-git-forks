#!/bin/bash

source constants.sh

rolecheck_data="roledata.sql"

###########################
# 	TASK ROLECHECK TESTS
###########################
echo "testing: git task rolecheck"

# TEST  1 --- rolecheck001 --- Creating a task without permissions
cat > "$input/rolecheck001.in" << \EOF
EOF
cat > "$output/rolecheck001.out" << \EOF
You haven't enought permissions to do this action.
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low' 'rolecheck001' 'no-permission'

# TEST  2 --- rolecheck002 --- Assigning a task without permissions
cat > "$input/rolecheck002.in" << \EOF
EOF
cat > "$output/rolecheck002.out" << \EOF
You haven't enought permissions to do this action.
EOF
./launch-test.sh 'git task -a -i 1 --user --add="user1"' 'rolecheck002' 'no-permission'

# TEST 3 --- rolecheck003 --- Deleting a task without permissions
cat > "$input/rolecheck003.in" << \EOF











EOF
cat > "$output/rolecheck003.out" << \EOF
You haven't enought permissions to do this action.
EOF
./launch-test.sh 'git task -d' 'rolecheck003' 'no-permission'

# TEST  4 --- rolecheck004 --- Linking files to task without permissions
cat > "$input/rolecheck004.in" << \EOF
EOF
cat > "$output/rolecheck004.out" << \EOF
You haven't enought permissions to do this action.
EOF
./launch-test.sh 'git task -l -i 1 --file --add="f1"' 'rolecheck004' 'no-permission'

# TEST 5 --- rolecheck005 --- Searching tasks without permissions
cat > "$input/rolecheck005.in" << \EOF











EOF
cat > "$output/rolecheck005.out" << \EOF
You haven't enought permissions to do this action.
EOF
./launch-test.sh 'git task -r' 'rolecheck005' 'no-permission'

# TEST 6 --- rolecheck006 --- Updating tasks without permissions
cat > "$input/rolecheck006.in" << \EOF











EOF
cat > "$output/rolecheck006.out" << \EOF
You haven't enought permissions to do this action.
EOF
./launch-test.sh 'git task -u -n nuevo -n nuevo' 'rolecheck006' 'no-permission'

# TEST  7 --- rolecheck007 --- Creating a task without a role
cat > "$input/rolecheck007.in" << \EOF
EOF
cat > "$output/rolecheck007.out" << \EOF
You haven't been assigned a role.
EOF
./launch-test.sh 'git task -c -n nueva_tarea -s new -t test -p low' 'rolecheck007' 'no-roles'

# TEST  8 --- rolecheck008 --- Assigning a task without a role
cat > "$input/rolecheck008.in" << \EOF
EOF
cat > "$output/rolecheck008.out" << \EOF
You haven't been assigned a role.
EOF
./launch-test.sh 'git task -a -i 1 --user --add="user1"' 'rolecheck008' 'no-roles'

# TEST 9 --- rolecheck009 --- Deleting a task without a role
cat > "$input/rolecheck009.in" << \EOF











EOF
cat > "$output/rolecheck009.out" << \EOF
You haven't been assigned a role.
EOF
./launch-test.sh 'git task -d' 'rolecheck009' 'no-roles'

# TEST  10 --- rolecheck010 --- Linking files to task without a role
cat > "$input/rolecheck010.in" << \EOF
EOF
cat > "$output/rolecheck010.out" << \EOF
You haven't been assigned a role.
EOF
./launch-test.sh 'git task -l -i 1 --file --add="f1"' 'rolecheck010' 'no-roles'

# TEST 11 --- rolecheck011 --- Searching tasks without a role
cat > "$input/rolecheck011.in" << \EOF











EOF
cat > "$output/rolecheck011.out" << \EOF
You haven't been assigned a role.
EOF
./launch-test.sh 'git task -r' 'rolecheck011' 'no-roles'

# TEST 12 --- rolecheck012 --- Updating tasks without a role
cat > "$input/rolecheck012.in" << \EOF











EOF
cat > "$output/rolecheck012.out" << \EOF
You haven't been assigned a role.
EOF
./launch-test.sh 'git task -u -n nuevo -n nuevo' 'rolecheck012' 'no-roles'

