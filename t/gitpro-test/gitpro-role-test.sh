#!/bin/bash

cat > "insert-data.sh" << \EOF
sqlite3 ../../.git/gitpro.db -batch < test-data.sql
exit 0
EOF

chmod +x init-test-user.sh
./init-test-user.sh

echo '***********************************'
echo '   Starting role creation tests'
echo '***********************************'
./gp-rcreate.sh
./clean-db.sh
echo '***********************************'
echo '   Starting role reading tests'
echo '***********************************'
./gp-rread.sh
./clean-db.sh
echo '***********************************'
echo '   Starting role deletion tests'
echo '***********************************'
./gp-rdelete.sh
./clean-db.sh
echo '***********************************'
echo '   Starting role update tests'
echo '***********************************'
./gp-rupdate.sh
./clean-db.sh
echo '***********************************'
echo '   Starting role assign tests'
echo '***********************************'
./gp-rassign.sh
./clean-db.sh

echo 'Your username has been changed to run this tests...'
echo 'Use git config --global to update your username'

chmod +x end-test-user.sh
./end-test-user.sh

rm test-data.sql
rm insert-data.sh
