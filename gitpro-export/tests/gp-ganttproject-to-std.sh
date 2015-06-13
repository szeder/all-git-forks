#!/bin/bash

input="test-files"

###########################
# GANNTPROJECT TO STD TESTS
###########################
echo "testing: ganttproject-to-std"

###########################################
#Test 001 : Tasks, users and assignations
###########################################
cat > "expected" <<\EOF
id
0
name
tarea_0
est-date-end
18/06/2015
real-date-ini
15/06/2015
est-time
32
id
1
name
tarea_1
est-date-ini
17/06/2015
est-date-end
23/06/2015
est-time
56
uname
juan
uname
pepe
uname
anabel
auname
juan
atid
0
auname
pepe
atid
0
EOF

./../compiled/ganttproject-to-std < test-files/ganttproject-to-std001.gan
./check.sh "ganttproject-to-std001" "standard-file" "expected"

###########################################
#Test 002 : Tasks
###########################################
cat > "expected" <<\EOF
id
0
name
tarea_0
real-date-ini
15/06/2015
real-date-end
18/06/2015
time
32
id
1
name
tarea_1
est-date-ini
17/06/2015
est-date-end
23/06/2015
est-time
56
EOF

./../compiled/ganttproject-to-std < test-files/ganttproject-to-std002.gan
./check.sh "ganttproject-to-std002" "standard-file" "expected"

###########################################
#Test 003 : Users
###########################################
cat > "expected" <<\EOF
uname
juan
uname
luis
uname
elena
EOF

./../compiled/ganttproject-to-std < test-files/ganttproject-to-std003.gan
./check.sh "ganttproject-to-std003" "standard-file" "expected"

###########################################
#Test 004 : Tasks and users
###########################################
cat > "expected" <<\EOF
id
0
name
tarea_0
est-date-ini
15/06/2015
est-date-end
18/06/2015
est-time
32
id
1
name
tarea_1
est-date-ini
17/06/2015
est-date-end
23/06/2015
est-time
56
uname
juan
uname
pepe
uname
anabel
EOF

./../compiled/ganttproject-to-std < test-files/ganttproject-to-std004.gan
./check.sh "ganttproject-to-std004" "standard-file" "expected"
