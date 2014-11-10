#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	ROLE SEARCH TESTS
###########################
echo "testing: git task -r"

# Insert previous data into tasks to run following tests until --TEST 95--
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

# TEST 1 --- rread001 --- Read valid role
cat > "$input/rread001.in" << \EOF











EOF
cat > "$output/rread001.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5	State: REJECTED	Priority: VERY HIGH	Type: CONFIGURATION
	Start	Estimated: 30/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: -1	Real: -1
	Description: my brief desc
	Notes: empty
1 | Name: task 1	State: NEW	Priority: HIGH	Type: TEST
	Start	Estimated: 20/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: 14	Real: 12
	Description: my desc
	Notes: my notes
2 | Name: task 2	State: IN PROGRESS	Priority: VERY LOW	Type: ANALYSIS
	Start	Estimated: empty	Real: 21/12/2014
	End  	Estimated: 24/12/2014	Real: empty
	Time 	Estimated: -1	Real: 12
	Description: empty
	Notes: my personal notes
3 | Name: task 3	State: IN PROGRESS	Priority: MAJOR	Type: MANAGEMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 26/12/2014	Real: 28/12/2014
	Time 	Estimated: 18	Real: -1
	Description: empty
	Notes: empty
4 | Name: task 4	State: REJECTED	Priority: URGENT	Type: DEVELOPMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 27/12/2014	Real: empty
	Time 	Estimated: 20	Real: 29
	Description: empty
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'rread001'
