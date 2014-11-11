#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	ROLE CREATION TESTS
###########################
echo "testing: git role -c"

cat > "clean-roles.sql" <<\EOF
DELETE FROM GP_ROL WHERE NOMBRE_ROL='TEST_A';
EOF

cat > "delete-roles.sh" << \EOF
sqlite3 ../../.git/gitpro.db -batch < clean-roles.sql
exit 0
EOF
chmod +x delete-roles.sh
./delete-roles.sh

rm clean-roles.sql delete-roles.sh

./clean-db.sh
# TEST  1 --- rcreate001 --- Creates a new successfull role
cat > "$input/rcreate001.in" << \EOF
EOF
cat > "$output/rcreate001.out" << \EOF
Role created successfully
EOF
./launch-test.sh 'git role -c -n TEST_A -p 1010101010' 'rcreate001'

./clean-db.sh
# TEST  2 --- rcreate002 --- Creates a duplicated role
cat > "$input/rcreate002.in" << \EOF
EOF
cat > "$output/rcreate002.out" << \EOF
Role name specified already exists
EOF
./launch-test.sh 'git role -c -n TEST_A -p 1010101010' 'rcreate002'

./clean-db.sh
# TEST  3 --- rcreate003 --- Creates a role with incorrect data
cat > "$input/rcreate003.in" << \EOF
EOF
cat > "$output/rcreate003.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -c -n TEST_A -p 1010111010101010' 'rcreate003'

./clean-db.sh
# TEST  4 --- rcreate004 --- Creates a role with incorrect data (letters in array bit)
cat > "$input/rcreate004.in" << \EOF
EOF
cat > "$output/rcreate004.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -c -n TEST_A -p 1101d01010' 'rcreate004'
