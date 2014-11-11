#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	ROLE Update TESTS
###########################
echo "testing: git role -u"

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

./clean-db.sh
# TEST  1 --- rupdate001 --- Creates a new successfull role
cat > "$input/rupdate001.in" << \EOF
EOF
cat > "$output/rupdate001.out" << \EOF
Role updated successfully
EOF
./launch-test.sh 'git role -u -n TEST_A -p 1010101010' 'rupdate001'

./clean-db.sh
# TEST  2 --- rupdate002 --- Creates a duplicated role
cat > "$input/rupdate002.in" << \EOF
EOF
cat > "$output/rupdate002.out" << \EOF
Role you're trying to update doesn't exists
EOF
./launch-test.sh 'git role -u -n INEXISTENT -p 1010101010' 'rupdate002'

./clean-db.sh
# TEST  3 --- rupdate003 --- Creates a role with incorrect data
cat > "$input/rupdate003.in" << \EOF
EOF
cat > "$output/rupdate003.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -u -n TEST_A -p 1010111010101010' 'rupdate003'

./clean-db.sh
# TEST  4 --- rupdate004 --- Creates a role with incorrect data (letters in array bit)
cat > "$input/rupdate004.in" << \EOF
EOF
cat > "$output/rupdate004.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -u -n TEST_A -p 1101d01010' 'rupdate004'
