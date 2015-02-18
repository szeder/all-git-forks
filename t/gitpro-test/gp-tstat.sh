#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	TASK STAT TESTS
###########################
echo "testing: git task --stat"

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

# TEST 1 --- stat001 --- Stats with some tasks in database
cat > "$input/stat001.in" << \EOF
EOF
cat > "$output/stat001.out" << \EOF
*****************************************************
               Project Statistics
*****************************************************
Project task completion : 0.00 %
	Time including rejected tasks
		- Estimated time : 52.00 hours
		- Real time      : 53.00 hours
	Time without rejected tasks
		- Total estimated time : 32.00 hours
		- Total real time      : 24.00 hours
Total tasks: 5
	Task number by state
		NEW : 1 (20.00 %)
		IN PROGRESS : 2 (40.00 %)
		REJECTED : 2 (40.00 %)
		RESOLVED : 0 (0.00 %)
	Task number by priority
		VERY LOW : 1 (20.00 %)
		LOW : 0 (0.00 %)
		MINOR : 0 (0.00 %)
		MAJOR : 1 (20.00 %)
		HIGH : 1 (20.00 %)
		VERY HIGH : 1 (20.00 %)
		URGENT : 1 (20.00 %)
		INMEDIATE : 0 (0.00 %)
		CRITICAL : 0 (0.00 %)
		BLOCKER : 0 (0.00 %)
	Task number by type
		ANALYSIS : 1 (20.00 %)
		DESIGN : 0 (0.00 %)
		MANAGEMENT : 1 (20.00 %)
		QUALITY : 0 (0.00 %)
		DEVELOPMENT : 1 (20.00 %)
		TEST : 1 (20.00 %)
		MAINTENANCE : 0 (0.00 %)
		SUPPORT : 0 (0.00 %)
		CONFIGURATION : 1 (20.00 %)
	Task assignations by user
		user1 : 0 (0.00 %)
		user2 : 0 (0.00 %)
		usertest : 0 (0.00 %)
	Time logged by user
		user1 : 0.00 hours
		user2 : 0.00 hours
		usertest : 0.00 hours
EOF
./launch-test.sh 'git task --stat' 'stat001'

./clean-db.sh
# TEST 2 --- stat002 --- Stats without tasks in database
cat > "$input/stat002.in" << \EOF
EOF
cat > "$output/stat002.out" << \EOF
*****************************************************
               Project Statistics
*****************************************************
Total tasks: 0
EOF
./launch-test.sh 'git task --stat' 'stat002'
