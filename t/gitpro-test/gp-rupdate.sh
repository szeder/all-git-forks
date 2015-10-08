#!/bin/bash

source constants.sh

###########################
# 	ROLE Update TESTS
###########################
echo "testing: git role -u"

# TEST  1 --- rupdate001 --- Creates a new successfull role
cat > "$input/rupdate001.in" << \EOF
EOF
cat > "$output/rupdate001.out" << \EOF
Role updated successfully
EOF
./launch-test.sh 'git role -u -n TEST_A -p 1010101010' 'rupdate001'

# TEST  2 --- rupdate002 --- Creates a duplicated role
cat > "$input/rupdate002.in" << \EOF
EOF
cat > "$output/rupdate002.out" << \EOF
Role you're trying to update doesn't exists
EOF
./launch-test.sh 'git role -u -n INEXISTENT -p 1010101010' 'rupdate002'

# TEST  3 --- rupdate003 --- Creates a role with incorrect data
cat > "$input/rupdate003.in" << \EOF
EOF
cat > "$output/rupdate003.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -u -n TEST_A -p 1010111010101010' 'rupdate003'

# TEST  4 --- rupdate004 --- Creates a role with incorrect data (letters in array bit)
cat > "$input/rupdate004.in" << \EOF
EOF
cat > "$output/rupdate004.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git role -u -n TEST_A -p 1101d01010' 'rupdate004'
