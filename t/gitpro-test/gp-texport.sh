#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	TASK EXPORT TESTS
###########################
echo "testing: git task --export"

# Insert previous data into tasks to run following tests
cat > "test-data.sql" << \EOF
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (1,'task 1','NEW','my desc','my notes','20/12/2014','21/12/2014',null,null,'HIGH','TEST',12,14);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (2,'task 2','IN PROGRESS',null,'my personal notes',null,'24/12/2014','21/12/2014',null,'VERY LOW','ANALYSIS',12,null);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (3,'task 3','IN PROGRESS',null,null,null,'26/12/2014',null,'28/12/2014','MAJOR','MANAGEMENT',null,18);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (4,'task 4','REJECTED',null,null,null,'27/12/2014',null,null,'URGENT','DEVELOPMENT',29,20);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (5,'task 5','REJECTED','my brief desc',null,'30/12/2014','21/12/2014',null,null,'VERY HIGH','CONFIGURATION',null,null);
.quit
EOF

chmod +x insert-data.sh

./clean-db.sh
./insert-data.sh

# TEST 1 --- export001 --- Export with no arguments
cat > "$input/export001.in" << \EOF
EOF
cat > "$output/export001.out" << \EOF
Output format not specified
EOF
./launch-test.sh 'git task --export' 'export001'

# TEST 2 --- export002 --- Export to inexistent output format
cat > "$input/export002.in" << \EOF
EOF
cat > "$output/export002.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
inexistent output format not available
available output formats are:
csv
ganttproject
json
EOF
./launch-test.sh 'git task --export --out inexistent' 'export002'

# TEST 3 --- export003 --- Export to csv
cat > "$input/export003.in" << \EOF
EOF
cat > "$output/export003.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
* Input file: gitpro-db.csv
* Input format: csv
+ Generating file in csv format
+ Output csv generated
EOF
./launch-test.sh 'git task --export --out csv' 'export003'
rm "../../gitpro-export/csv-task.csv"

# TEST 4 --- export004 --- Export to ganttproject
cat > "$input/export004.in" << \EOF
EOF
cat > "$output/export004.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
* Input file: gitpro-db.csv
* Input format: csv
+ Generating file in ganttproject format
+ Output ganttproject generated
EOF
./launch-test.sh 'git task --export --out ganttproject' 'export004'
rm "../../gitpro-export/ganttproject.gan"

# TEST 5 --- export005 --- Export to json
cat > "$input/export005.in" << \EOF
EOF
cat > "$output/export005.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
* Input file: gitpro-db.csv
* Input format: csv
+ Generating file in json format
+ Output json generated
EOF
./launch-test.sh 'git task --export --out json' 'export005'
rm "../../gitpro-export/json-task.json"
