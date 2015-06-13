#!/bin/bash

input="test-files"

###########################
# 	STD TO CSV TESTS
###########################
echo "testing: std-to-csv"

###########################################
#Test 001 : Tasks, users and assignations
###########################################
cat > "expected" <<\EOF
1,nombre tarea,nueva,mi descripcion,pequeña nota,20/10/2010,23/10/2010,15/11/2010,18/11/2010,alta,desarrollo,20,15
2,otra tarea,terminada,,,,,,,,,,
break
pepe,jefe
break
pepe,1
EOF

./../compiled/std-to-csv test-files/std-to-xxx001
./check.sh "std-to-csv001" "csv-task.csv" "expected"

###########################################
#Test 002 : Tasks
###########################################
cat > "expected" <<\EOF
1,nombre tarea,nueva,,,20/06/2010,23/07/2010,15/06/2010,18/07/2010,alta,desarrollo,,
break
break
EOF

./../compiled/std-to-csv test-files/std-to-xxx002
./check.sh "std-to-csv002" "csv-task.csv" "expected"

###########################################
#Test 003 : Users
###########################################
cat > "expected" <<\EOF
break
pepe,jefe
juan,empleado
break
EOF

./../compiled/std-to-csv test-files/std-to-xxx003
./check.sh "std-to-csv003" "csv-task.csv" "expected"

###########################################
#Test 004 : Tasks and users
###########################################
cat > "expected" <<\EOF
1,nombre tarea,nueva,,,20/06/2010,23/07/2010,15/06/2010,18/07/2010,alta,desarrollo,,
break
pepe,jefe
juan,empleado
break
EOF

./../compiled/std-to-csv test-files/std-to-xxx004
./check.sh "std-to-csv004" "csv-task.csv" "expected"
