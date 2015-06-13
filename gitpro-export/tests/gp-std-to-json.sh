#!/bin/bash

input="test-files"

###########################
# 	STD TO JSON TESTS
###########################
echo "testing: std-to-json"

###########################################
#Test 001 : Tasks, users and assignations
###########################################
cat > "expected" <<\EOF
{
"task": [
	{"task-id":"1", "name":"nombre tarea", "state":"nueva", "description":"mi descripcion", "notes":"pequeña nota", "estimated-start-date":"20/10/2010", "estimated-end-date":"23/10/2010", "real-start-date":"15/11/2010", "real-end-date":"18/11/2010", "priority":"alta", "type":"desarrollo", "real-duration-hours":"20" "estimated-duration-hours":"15", }
	{"task-id":"2", "name":"otra tarea", "state":"terminada", }
]
"user": [
	{"username":"pepe", "role":"jefe" }
]
"asig": [
	{"username":"pepe","task-id":"1" }
]
}
EOF

./../compiled/std-to-json test-files/std-to-xxx001
./check.sh "std-to-json001" "json-task.json" "expected"

###########################################
#Test 002 : Tasks
###########################################
cat > "expected" <<\EOF
{
"task": [
	{"task-id":"1", "name":"nombre tarea", "state":"nueva", "estimated-start-date":"20/06/2010", "estimated-end-date":"23/07/2010", "real-start-date":"15/06/2010", "real-end-date":"18/07/2010", "priority":"alta", "type":"desarrollo", }
]
"user": [
]
"asig": [
]
}
EOF

./../compiled/std-to-json test-files/std-to-xxx002
./check.sh "std-to-json002" "json-task.json" "expected"

###########################################
#Test 003 : Users
###########################################
cat > "expected" <<\EOF
{
"task": [
]
"user": [
	{"username":"pepe", "role":"jefe" }
	{"username":"juan", "role":"empleado" }
]
"asig": [
]
}
EOF

./../compiled/std-to-json test-files/std-to-xxx003
./check.sh "std-to-json003" "json-task.json" "expected"

###########################################
#Test 004 : Tasks and users
###########################################
cat > "expected" <<\EOF
{
"task": [
	{"task-id":"1", "name":"nombre tarea", "state":"nueva", "estimated-start-date":"20/06/2010", "estimated-end-date":"23/07/2010", "real-start-date":"15/06/2010", "real-end-date":"18/07/2010", "priority":"alta", "type":"desarrollo", }
]
"user": [
	{"username":"pepe", "role":"jefe" }
	{"username":"juan", "role":"empleado" }
]
"asig": [
]
}
EOF

./../compiled/std-to-json test-files/std-to-xxx004
./check.sh "std-to-json004" "json-task.json" "expected"
