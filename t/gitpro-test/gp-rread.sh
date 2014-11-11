#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	ROLE SEARCH TESTS
###########################
echo "testing: git role -r"

cat > "clean-roles.sql" <<\EOF
DELETE FROM GP_ROL WHERE NOMBRE_ROL='TEST_A';
DELETE FROM GP_ROL WHERE NOMBRE_ROL='TEST_B';
EOF

cat > "delete-roles.sh" << \EOF
sqlite3 ../../.git/gitpro.db -batch < clean-roles.sql
exit 0
EOF
chmod +x delete-roles.sh
./delete-roles.sh

rm clean-roles.sql delete-roles.sh

# Insert previous data into roles to run following tests
cat > "test-data.sql" << \EOF
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol) values ('TEST_A',1,1);
INSERT INTO GP_ROL(nombre_rol) values ('TEST_B');
EOF

chmod +x insert-data.sh

./clean-db.sh
./insert-data.sh

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
