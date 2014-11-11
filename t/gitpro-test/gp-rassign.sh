#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	ROLE ASSIGN TESTS
###########################
echo "testing: git role -a"

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


### ASSIGN ROLE TESTS

./clean-db.sh
./insert-data.sh
# TEST  1 --- rassign001 --- assign a role (incorrect data)
cat > "$input/rassign001.in" << \EOF
EOF
cat > "$output/rassign001.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -a --user' 'rassign001'

./clean-db.sh
./insert-data.sh
# TEST  2 --- rassign002 --- incorrect data (role)
cat > "$input/rassign002.in" << \EOF
EOF
cat > "$output/rassign002.out" << \EOF
Role you're trying to assign doesn't exists
EOF
./launch-test.sh 'git role -a -n inexistent --user --rm="usertest"' 'rassign002'

./clean-db.sh
./insert-data.sh
# TEST  3 --- rassign003 --- incorrect data (user)
cat > "$input/rassign003.in" << \EOF
EOF
cat > "$output/rassign003.out" << \EOF
User you're trying to assign doesn't exists
EOF
./launch-test.sh 'git role -a -n TEST_A --user --rm="inexistent"' 'rassign003'

./clean-db.sh
./insert-data.sh
# TEST  4 --- rassign004 --- assign a role (ok)
cat > "$input/rassign004.in" << \EOF
EOF
cat > "$output/rassign004.out" << \EOF
Role assigned to following users:
+ usertest
EOF
./launch-test.sh 'git role -a -n TEST_A --user --add="usertest"' 'rassign004'

./clean-db.sh
./insert-data.sh
# TEST  5 --- rassign005 --- deassign a role (ok)
cat > "$input/rassign005.in" << \EOF
EOF
cat > "$output/rassign005.out" << \EOF
Role deassigned to following users and set to default (PUBLIC):
- usertest
EOF
./launch-test.sh 'git role -a -n TEST_A --user --rm="usertest"' 'rassign005'

# Insert previous data into roles to run following tests
cat > "test-data.sql" << \EOF
UPDATE GP_USUARIO SET nombre_rol_usuario='EXAMPLE' WHERE nombre_usuario='usertest';
EOF

chmod +x insert-data.sh
./insert-data.sh
