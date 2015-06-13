#!/bin/bash

input="test-files"

###########################
# 	CSV TO STD TESTS
###########################
echo "testing: csv-to-std"

###########################################
#Test 001 : Tasks, users and assignations
###########################################
cat > "expected" <<\EOF
id
1
name
"tarea 1"
state
NEW
priority
HIGH
type
TEST
id
2
name
"tarea 2"
state
NEW
priority
HIGH
type
TEST
id
3
name
"tarea 3"
state
NEW
priority
HIGH
type
TEST
uname
pepe
urole
PUBLIC
uname
juan
urole
ALL
auname
juan
atid
1
auname
pepe
atid
2
EOF

./../compiled/csv-to-std < test-files/csv-to-std001.csv
./check.sh "csv-to-std001" "standard-file" "expected"

###########################################
#Test 002 : Tasks
###########################################
cat > "expected" <<\EOF
id
1
name
"tarea 1"
state
NEW
notes
"mis notas"
est-date-end
13/12/1982
priority
HIGH
type
TEST
id
2
name
"tarea 2"
state
NEW
desc
"mi desc"
notes
"nota"
est-date-ini
12/10/1999
est-date-end
13/11/1999
real-date-ini
13/12/2005
real-date-end
18/05/2010
priority
HIGH
type
TEST
time
10
est-time
20
EOF

./../compiled/csv-to-std < test-files/csv-to-std002.csv
./check.sh "csv-to-std002" "standard-file" "expected"

###########################################
#Test 003 : Users
###########################################
cat > "expected" <<\EOF
uname
pepe
urole
PUBLIC
uname
juan
urole
ALL
uname
anabel
urole
develop
EOF

./../compiled/csv-to-std < test-files/csv-to-std003.csv
./check.sh "csv-to-std003" "standard-file" "expected"

###########################################
#Test 004 : Tasks and users
###########################################
cat > "expected" <<\EOF
id
1
name
"tarea 1"
state
NEW
id
2
name
"tarea 2"
state
NEW
id
3
name
"tarea 3"
state
NEW
uname
pepe
urole
firstrole
uname
juan
urole
secondrole
EOF

./../compiled/csv-to-std < test-files/csv-to-std004.csv
./check.sh "csv-to-std004" "standard-file" "expected"
