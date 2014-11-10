#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	ROLE Delete TESTS
###########################
echo "testing: git task -d"

# Insert previous data into tasks after each test until --TEST 95--
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

# TEST 1 --- rdelete001 --- Delete a valid role
cat > "$input/rdelete001.in" << \EOF











EOF
cat > "$output/rdelete001.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: - Removing asignations and asociations to task 1 ...
- Task 1 prepared to be removed
- Removing asignations and asociations to task 2 ...
- Task 2 prepared to be removed
- Removing asignations and asociations to task 3 ...
- Task 3 prepared to be removed
- Removing asignations and asociations to task 4 ...
- Task 4 prepared to be removed
- Removing asignations and asociations to task 5 ...
- Task 5 prepared to be removed
** All selected task removed successfully
EOF
./launch-test.sh 'git task -d' 'rdelete001'
