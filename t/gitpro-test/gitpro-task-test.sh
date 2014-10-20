#!/bin/bash

cat > "insert-data.sh" << \EOF
sqlite3 ../../.git/gitpro.db -batch < test-data.sql
exit 0
EOF

echo '***********************************'
echo '   Starting task creation tests'
echo '***********************************'
./gp-tcreate.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task reading tests'
echo '***********************************'
./gp-tread.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task deletion tests'
echo '***********************************'
./gp-tdelete.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task update tests'
echo '***********************************'
./gp-tupdate.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task assign tests'
echo '***********************************'
./gp-tassign.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task link tests'
echo '***********************************'
./gp-tlink.sh
./clean-db.sh

rm test-data.sql
rm insert-data.sh
