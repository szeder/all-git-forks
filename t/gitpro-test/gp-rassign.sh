#!/bin/bash

source constants.sh

###########################
# 	ROLE ASSIGN TESTS
###########################
echo "testing: git role -a"

### ASSIGN ROLE TESTS

# TEST  1 --- rassign001 --- assign a role (incorrect data)
cat > "$input/rassign001.in" << \EOF
EOF
cat > "$output/rassign001.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -a --user' 'rassign001'

# TEST  2 --- rassign002 --- incorrect data (role)
cat > "$input/rassign002.in" << \EOF
EOF
cat > "$output/rassign002.out" << \EOF
Role you're trying to assign doesn't exists
EOF
./launch-test.sh 'git role -a -n inexistent --user --rm="usertest"' 'rassign002'

# TEST  3 --- rassign003 --- incorrect data (user)
cat > "$input/rassign003.in" << \EOF
EOF
cat > "$output/rassign003.out" << \EOF
User you're trying to assign doesn't exists
EOF
./launch-test.sh 'git role -a -n TEST_A --user --rm="inexistent"' 'rassign003'

# TEST  4 --- rassign004 --- assign a role (ok)
cat > "$input/rassign004.in" << \EOF
EOF
cat > "$output/rassign004.out" << \EOF
Role assigned to following users:
+ usertest
EOF
./launch-test.sh 'git role -a -n TEST_A --user --add="usertest"' 'rassign004'

# TEST  5 --- rassign005 --- deassign a role (ok)
cat > "$input/rassign005.in" << \EOF
EOF
cat > "$output/rassign005.out" << \EOF
Role deassigned to following users and set to default (PUBLIC):
- usertest
EOF
./launch-test.sh 'git role -a -n TEST_A --user --rm="usertest"' 'rassign005'
