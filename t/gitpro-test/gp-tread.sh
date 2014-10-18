#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	TASK SEARCH TESTS
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

cat > "insert-data.sh" << \EOF
sqlite3 ../../gitpro.db -batch < test-data.sql
exit 0
EOF

chmod +x insert-data.sh

./clean-db.sh
./insert-data.sh

# TEST 1 --- read001 --- Search all task (no filters) (multiple tasks exists)
cat > "$input/read001.in" << \EOF











EOF
cat > "$output/read001.out" << \EOF
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
./launch-test.sh 'git task -r' 'read001'

# TEST 2 --- read002 --- Search one task (id filter) (task exists)
cat > "$input/read002.in" << \EOF
3










EOF
cat > "$output/read002.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
3 | Name: task 3	State: IN PROGRESS	Priority: MAJOR	Type: MANAGEMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 26/12/2014	Real: 28/12/2014
	Time 	Estimated: 18	Real: -1
	Description: empty
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read002'

# TEST 3 --- read003 --- Search task by name (had to match exactly) (one task finded)
cat > "$input/read003.in" << \EOF

task 5









EOF
cat > "$output/read003.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5	State: REJECTED	Priority: VERY HIGH	Type: CONFIGURATION
	Start	Estimated: 30/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: -1	Real: -1
	Description: my brief desc
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read003'

# TEST 4 --- read004 --- Search task by state (multiple task matches) (minus)
cat > "$input/read004.in" << \EOF


in progress








EOF
cat > "$output/read004.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
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
EOF
./launch-test.sh 'git task -r' 'read004'

# TEST 5 --- read005 --- Search task by state (multiple task matches) (mayus)
cat > "$input/read005.in" << \EOF


IN PROGRESS








EOF
cat > "$output/read005.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
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
EOF
./launch-test.sh 'git task -r' 'read005'

# TEST 6 --- read006 --- Search task by state (multiple task matches) (mayus & minus)
cat > "$input/read006.in" << \EOF


iN proGrEss








EOF
cat > "$output/read006.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
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
EOF
./launch-test.sh 'git task -r' 'read006'

# TEST 7 --- read007 --- Search by id (invalid id)
cat > "$input/read007.in" << \EOF
hola










EOF
cat > "$output/read007.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read007'

# TEST 8 --- read008 --- Search by id (no task matching)
cat > "$input/read008.in" << \EOF
20










EOF
cat > "$output/read008.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read008'

# TEST 9 --- read009 --- Search by name (only part of name matches)
cat > "$input/read009.in" << \EOF

inexistent









EOF
cat > "$output/read009.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read009'

# TEST 10 --- read010 --- Search task by name (part of name matching only with one task)
cat > "$input/read010.in" << \EOF

5









EOF
cat > "$output/read010.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read010'

# TEST 11 --- read011 --- Search task by name (part of name matching with multiple task names)
cat > "$input/read011.in" << \EOF

task









EOF
cat > "$output/read011.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read011'

# TEST 12 --- read012 --- Search task by state (only one matching) (minus)
cat > "$input/read012.in" << \EOF


new








EOF
cat > "$output/read012.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1	State: NEW	Priority: HIGH	Type: TEST
	Start	Estimated: 20/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: 14	Real: 12
	Description: my desc
	Notes: my notes
EOF
./launch-test.sh 'git task -r' 'read012'

# TEST 13 --- read013 --- Search task by state (only one matching) (mayus)
cat > "$input/read013.in" << \EOF


NEW








EOF
cat > "$output/read013.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1	State: NEW	Priority: HIGH	Type: TEST
	Start	Estimated: 20/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: 14	Real: 12
	Description: my desc
	Notes: my notes
EOF
./launch-test.sh 'git task -r' 'read013'

# TEST 14 --- read014 --- Search task by state (only one matching) (mixed mayus and minus)
cat > "$input/read014.in" << \EOF


New








EOF
cat > "$output/read014.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1	State: NEW	Priority: HIGH	Type: TEST
	Start	Estimated: 20/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: 14	Real: 12
	Description: my desc
	Notes: my notes
EOF
./launch-test.sh 'git task -r' 'read014'

# TEST 15 --- read015 --- Search task by state (invalid / inexistent state)
cat > "$input/read015.in" << \EOF


inexistent








EOF
cat > "$output/read015.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read015'

# TEST 16 --- read016 --- Search task by estimated init date (invalid date letters)
cat > "$input/read016.in" << \EOF



date







EOF
cat > "$output/read016.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read016'

# TEST 17 --- read017 --- Search task by estimated init date (invalid day in date)
cat > "$input/read017.in" << \EOF



50/12/2014







EOF
cat > "$output/read017.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read017'

# TEST 18 --- read018 --- Search task by estimated init date (invalid month in date)
cat > "$input/read018.in" << \EOF



15/16/2014







EOF
cat > "$output/read018.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read018'

# TEST 19 --- read019 --- Search task by estimated init date (invalid year in date)
cat > "$input/read019.in" << \EOF



15/05/23







EOF
cat > "$output/read019.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read019'

# TEST 20 --- read020 --- Search task by estimated init date (invalid day in date) (negative)
cat > "$input/read020.in" << \EOF



-12/06/2014







EOF
cat > "$output/read020.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read020'

# TEST 21 --- read021 --- Search task by estimated init date (invalid day in date) (zero)
cat > "$input/read021.in" << \EOF



00/06/2014







EOF
cat > "$output/read021.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read021'

# TEST 22 --- read022 --- Search task by estimated  init date (invalid month in date) (zero)
cat > "$input/read022.in" << \EOF



15/00/2014







EOF
cat > "$output/read022.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read022'

# TEST 23 --- read023 --- Search task by estimated init date (invalid month in date) (negative)
cat > "$input/read023.in" << \EOF



15/-11/2014







EOF
cat > "$output/read023.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read023'

# TEST 24 --- read024 --- Search task by estimated init date (invalid year in date) (over four numbers)
cat > "$input/read024.in" << \EOF



15/05/20146







EOF
cat > "$output/read024.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read024'

# TEST 25 --- read025 --- Search task by estimated init date (invalid year in date) (negative)
cat > "$input/read025.in" << \EOF



15/11/-2014







EOF
cat > "$output/read025.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read025'

# TEST 26 --- read026 --- Search task by estimated final date (invalid date letters)
cat > "$input/read026.in" << \EOF




date






EOF
cat > "$output/read026.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read026'

# TEST 27 --- read027 --- Search task by estimated final date (invalid day in date)
cat > "$input/read027.in" << \EOF




50/12/2014







EOF
cat > "$output/read027.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read027'

# TEST 28 --- read028 --- Search task by estimated final date (invalid month in date)
cat > "$input/read028.in" << \EOF




15/16/2014







EOF
cat > "$output/read028.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read028'

# TEST 29 --- read029 --- Search task by estimated final date (invalid year in date)
cat > "$input/read029.in" << \EOF




15/05/23







EOF
cat > "$output/read029.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read029'

# TEST 30 --- read030 --- Search task by estimated final date (invalid day in date) (negative)
cat > "$input/read030.in" << \EOF




-12/10/2014






EOF
cat > "$output/read030.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read030'

# TEST 31 --- read031 --- Search task by estimated final date (invalid day in date) (zero)
cat > "$input/read031.in" << \EOF




00/05/2014






EOF
cat > "$output/read031.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read031'

# TEST 32 --- read032 --- Search task by estimated final date (invalid month in date) (zero)
cat > "$input/read032.in" << \EOF




15/00/2014






EOF
cat > "$output/read032.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read032'

# TEST 33 --- read033 --- Search task by estimated final date (invalid month in date) (negative)
cat > "$input/read033.in" << \EOF




15/-11/2014






EOF
cat > "$output/read033.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read033'

# TEST 34 --- read034 --- Search task by estimated final date (invalid year in date) (over four numbers)
cat > "$input/read034.in" << \EOF




15/05/20146






EOF
cat > "$output/read034.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read034'

# TEST 35 --- read035 --- Search task by estimated final date (invalid year in date) (negative)
cat > "$input/read035.in" << \EOF




15/11/-2014






EOF
cat > "$output/read035.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read035'

# TEST 36 --- read036 --- Search task by real init date (invalid date letters)
cat > "$input/read036.in" << \EOF





date





EOF
cat > "$output/read036.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read036'

# TEST 37 --- read037 --- Search task by real init date (invalid day in date)
cat > "$input/read037.in" << \EOF





50/12/2014






EOF
cat > "$output/read037.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read037'

# TEST 38 --- read038 --- Search task by real init date (invalid month in date)
cat > "$input/read038.in" << \EOF





15/16/2014






EOF
cat > "$output/read038.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read038'

# TEST 39 --- read039 --- Search task by real init date (invalid year in date)
cat > "$input/read039.in" << \EOF





15/05/23






EOF
cat > "$output/read039.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read039'

# TEST 40 --- read040 --- Search task by real init date (invalid day in date) (negative)
cat > "$input/read040.in" << \EOF





-12/10/2014





EOF
cat > "$output/read040.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read040'

# TEST 41 --- read041 --- Search task by real init date (invalid day in date) (zero)
cat > "$input/read041.in" << \EOF





00/11/2014





EOF
cat > "$output/read041.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read041'

# TEST 42 --- read042 --- Search task by real init date (invalid month in date) (zero)
cat > "$input/read042.in" << \EOF





15/00/2014





EOF
cat > "$output/read042.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read042'

# TEST 43 --- read043 --- Search task by real init date (invalid month in date) (negative)
cat > "$input/read043.in" << \EOF





15/-11/2014





EOF
cat > "$output/read043.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read043'

# TEST 44 --- read044 --- Search task by real init date (invalid year in date) (over four numbers)
cat > "$input/read044.in" << \EOF





15/05/20146





EOF
cat > "$output/read044.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read044'

# TEST 45 --- read045 --- Search task by real init date (invalid year in date) (negative)
cat > "$input/read045.in" << \EOF





15/11/-2014





EOF
cat > "$output/read045.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read045'

# TEST 46 --- read046 --- Search task by real final date (invalid date letters)
cat > "$input/read046.in" << \EOF






date




EOF
cat > "$output/read046.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read046'

# TEST 47 --- read047 --- Search task by real final date (invalid day in date)
cat > "$input/read047.in" << \EOF






50/12/2014





EOF
cat > "$output/read047.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read047'

# TEST 48 --- read048 --- Search task by real final date (invalid month in date)
cat > "$input/read048.in" << \EOF






15/16/2014





EOF
cat > "$output/read048.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read048'

# TEST 49 --- read049 --- Search task by real final date (invalid year in date)
cat > "$input/read049.in" << \EOF






15/05/23





EOF
cat > "$output/read049.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read049'

# TEST 50 --- read050 --- Search task by real final date (invalid day in date) (negative)
cat > "$input/read050.in" << \EOF






-12/10/2014




EOF
cat > "$output/read050.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read050'

# TEST 51 --- read051 --- Search task by real init date (invalid day in date) (zero)
cat > "$input/read051.in" << \EOF






00/11/2014




EOF
cat > "$output/read051.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read051'

# TEST 52 --- read052 --- Search task by real final date (invalid month in date) (zero)
cat > "$input/read052.in" << \EOF






15/00/2014




EOF
cat > "$output/read052.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read052'

# TEST 53 --- read053 --- Search task by real final date (invalid month in date) (negative)
cat > "$input/read053.in" << \EOF






15/-11/2014




EOF
cat > "$output/read053.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read053'

# TEST 54 --- read054 --- Search task by real final date (invalid year in date) (over four numbers)
cat > "$input/read054.in" << \EOF






15/05/20146




EOF
cat > "$output/read054.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read054'

# TEST 55 --- read055 --- Search task by real final date (invalid year in date) (negative)
cat > "$input/read055.in" << \EOF






15/11/-2014




EOF
cat > "$output/read055.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read055'

# TEST 56 --- read056 --- Search by estimated init date (valid) (one task matching)
cat > "$input/read056.in" << \EOF



30/12/2014







EOF
cat > "$output/read056.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5	State: REJECTED	Priority: VERY HIGH	Type: CONFIGURATION
	Start	Estimated: 30/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: -1	Real: -1
	Description: my brief desc
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read056'

# TEST 57 --- read057 --- Search by estimated final date (valid) (one task matching)
cat > "$input/read057.in" << \EOF




24/12/2014






EOF
cat > "$output/read057.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2	State: IN PROGRESS	Priority: VERY LOW	Type: ANALYSIS
	Start	Estimated: empty	Real: 21/12/2014
	End  	Estimated: 24/12/2014	Real: empty
	Time 	Estimated: -1	Real: 12
	Description: empty
	Notes: my personal notes
EOF
./launch-test.sh 'git task -r' 'read057'

# TEST 58 --- read058 --- Search by real init date (valid) (one task matching)
cat > "$input/read058.in" << \EOF





21/12/2014





EOF
cat > "$output/read058.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2	State: IN PROGRESS	Priority: VERY LOW	Type: ANALYSIS
	Start	Estimated: empty	Real: 21/12/2014
	End  	Estimated: 24/12/2014	Real: empty
	Time 	Estimated: -1	Real: 12
	Description: empty
	Notes: my personal notes
EOF
./launch-test.sh 'git task -r' 'read058'

# TEST 59 --- read059 --- Search by real final date (valid) (one task matching)
cat > "$input/read059.in" << \EOF






28/12/2014




EOF
cat > "$output/read059.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
3 | Name: task 3	State: IN PROGRESS	Priority: MAJOR	Type: MANAGEMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 26/12/2014	Real: 28/12/2014
	Time 	Estimated: 18	Real: -1
	Description: empty
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read059'

# TEST 60 --- read060 --- Search by estimated init date (valid) (no task matching)
cat > "$input/read060.in" << \EOF



05/05/2014







EOF
cat > "$output/read060.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read060'

# TEST 61 --- read061 --- Search by estimated final date (valid) (no task matching)
cat > "$input/read061.in" << \EOF




05/06/2014






EOF
cat > "$output/read061.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read061'

# TEST 62 --- read062 --- Search by real init date (valid) (no task matching)
cat > "$input/read062.in" << \EOF





05/08/2014





EOF
cat > "$output/read062.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read062'

# TEST 63 --- read063 --- Search by real final date (valid) (no task matching)
cat > "$input/read063.in" << \EOF






28/02/2014




EOF
cat > "$output/read063.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read063'

# TEST 64 --- read064 --- Search by priority (inexistent prior)
cat > "$input/read064.in" << \EOF







inexistent



EOF
cat > "$output/read064.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read064'

# TEST 65 --- read065 --- Search by priority (valid prior) (minus)
cat > "$input/read065.in" << \EOF







very high



EOF
cat > "$output/read065.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5	State: REJECTED	Priority: VERY HIGH	Type: CONFIGURATION
	Start	Estimated: 30/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: -1	Real: -1
	Description: my brief desc
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read065'

# TEST 66 --- read066 --- Search by priority (valid prior) (no tasks) (minus)
cat > "$input/read066.in" << \EOF







blocker



EOF
cat > "$output/read066.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read066'

# TEST 67 --- read067 --- Search by priority (valid prior) (mayus)
cat > "$input/read067.in" << \EOF







VERY HIGH



EOF
cat > "$output/read067.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5	State: REJECTED	Priority: VERY HIGH	Type: CONFIGURATION
	Start	Estimated: 30/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: -1	Real: -1
	Description: my brief desc
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read067'

# TEST 68 --- read068 --- Search by priority (valid prior) (no tasks) (mayus)
cat > "$input/read068.in" << \EOF







BLOCKER



EOF
cat > "$output/read068.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read068'

# TEST 69 --- read069 --- Search by priority (valid prior) (mixed [mayus | minus] )
cat > "$input/read069.in" << \EOF







veRy hIgH



EOF
cat > "$output/read069.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
5 | Name: task 5	State: REJECTED	Priority: VERY HIGH	Type: CONFIGURATION
	Start	Estimated: 30/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: -1	Real: -1
	Description: my brief desc
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read069'

# TEST 70 --- read070 --- Search by priority (valid prior) (no tasks) (mixed [mayus | minus])
cat > "$input/read070.in" << \EOF







blOcKEr



EOF
cat > "$output/read070.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read070'

# TEST 71 --- read071 --- Search by type (inexistent type)
cat > "$input/read071.in" << \EOF








inexistent


EOF
cat > "$output/read071.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read071'

# TEST 72 --- read072 --- Search by type (valid type) (minus)
cat > "$input/read072.in" << \EOF








test


EOF
cat > "$output/read072.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1	State: NEW	Priority: HIGH	Type: TEST
	Start	Estimated: 20/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: 14	Real: 12
	Description: my desc
	Notes: my notes
EOF
./launch-test.sh 'git task -r' 'read072'

# TEST 73 --- read073 --- Search by type (valid type) (no tasks) (minus)
cat > "$input/read073.in" << \EOF








support


EOF
cat > "$output/read073.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read073'

# TEST 74 --- read074 --- Search by type (valid type) (mayus)
cat > "$input/read074.in" << \EOF








TEST


EOF
cat > "$output/read074.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1	State: NEW	Priority: HIGH	Type: TEST
	Start	Estimated: 20/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: 14	Real: 12
	Description: my desc
	Notes: my notes
EOF
./launch-test.sh 'git task -r' 'read074'

# TEST 75 --- read075 --- Search by type (valid type) (no tasks) (mayus)
cat > "$input/read075.in" << \EOF








SUPPORT


EOF
cat > "$output/read075.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read075'

# TEST 76 --- read076 --- Search by type (valid type) (mixed [mayus | minus] )
cat > "$input/read076.in" << \EOF








tEsT


EOF
cat > "$output/read076.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
1 | Name: task 1	State: NEW	Priority: HIGH	Type: TEST
	Start	Estimated: 20/12/2014	Real: empty
	End  	Estimated: 21/12/2014	Real: empty
	Time 	Estimated: 14	Real: 12
	Description: my desc
	Notes: my notes
EOF
./launch-test.sh 'git task -r' 'read076'

# TEST 77 --- read077 --- Search by type (valid type) (no tasks) (mixed [mayus | minus])
cat > "$input/read077.in" << \EOF








suPPorT


EOF
cat > "$output/read077.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read077'

# TEST 78 --- read078 --- Search by estimated time (invalid data)
cat > "$input/read078.in" << \EOF









invalid

EOF
cat > "$output/read078.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read078'

# TEST 79 --- read079 --- Search by real time (invalid data)
cat > "$input/read079.in" << \EOF










invalid
EOF
cat > "$output/read079.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read079'

# TEST 80 --- read080 --- Search by estimated time (valid data)
cat > "$input/read080.in" << \EOF









18

EOF
cat > "$output/read080.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
3 | Name: task 3	State: IN PROGRESS	Priority: MAJOR	Type: MANAGEMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 26/12/2014	Real: 28/12/2014
	Time 	Estimated: 18	Real: -1
	Description: empty
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read080'

# TEST 81 --- read081 --- Search by real time (valid data)
cat > "$input/read081.in" << \EOF










29
EOF
cat > "$output/read081.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
4 | Name: task 4	State: REJECTED	Priority: URGENT	Type: DEVELOPMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 27/12/2014	Real: empty
	Time 	Estimated: 20	Real: 29
	Description: empty
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read081'

# TEST 82 --- read082 --- Search by estimated time (valid data number) (overflow [more than 10 digits])
cat > "$input/read082.in" << \EOF









12345678901

EOF
cat > "$output/read082.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read082'

# TEST 83 --- read083 --- Search by real time (valid data number) (overflow [more than 10 digits])
cat > "$input/read083.in" << \EOF










12345678901
EOF
cat > "$output/read083.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read083'

# TEST 84 --- read084 --- Search by estimated time (valid data number) (zero)
cat > "$input/read084.in" << \EOF









0

EOF
cat > "$output/read084.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read084'

# TEST 85 --- read085 --- Search by real time (valid data number) (zero)
cat > "$input/read085.in" << \EOF










0
EOF
cat > "$output/read085.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read085'

# TEST 86 --- read086 --- Search by estimated time (valid data number) (negative limit -1)
cat > "$input/read086.in" << \EOF









-1

EOF
cat > "$output/read086.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read086'

# TEST 87 --- read087 --- Search by real time (valid data number) (negative limit -1)
cat > "$input/read087.in" << \EOF










-1
EOF
cat > "$output/read087.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read087'

# TEST 88 --- read088 --- Search by estimated time (valid data number) (negative other)
cat > "$input/read088.in" << \EOF









-18

EOF
cat > "$output/read088.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read088'

# TEST 89 --- read089 --- Search by real time (valid data number) (negative other)
cat > "$input/read089.in" << \EOF










-29
EOF
cat > "$output/read089.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -r' 'read089'

# TEST 90 --- read090 --- Search by estimated time (valid data number) (no tasks)
cat > "$input/read090.in" << \EOF









46

EOF
cat > "$output/read090.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read090'

# TEST 91 --- read091 --- Search by real time (valid data number) (no tasks)
cat > "$input/read091.in" << \EOF










84
EOF
cat > "$output/read091.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read091'

# TEST 92 --- read092 --- Search task by state (valid state) (no tasks match) (minus)
cat > "$input/read092.in" << \EOF


resolved








EOF
cat > "$output/read092.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read092'

# TEST 93 --- read093 --- Search task by state (valid state) (no tasks match) (mayus)
cat > "$input/read093.in" << \EOF


RESOLVED








EOF
cat > "$output/read093.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read093'

# TEST 94 --- read094 --- Search task by state (valid state) (no tasks match) (mixed [mayus | minus])
cat > "$input/read094.in" << \EOF


ResOlVEd








EOF
cat > "$output/read094.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read094'

./clean-db.sh
cat > "test-data.sql" << \EOF
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (2,'task 2','IN PROGRESS',null,'my personal notes',null,'24/12/2014','21/12/2014',null,'VERY LOW','ANALYSIS',12,null);
.quit
EOF
./insert-data.sh
# TEST 95 --- read095 --- Search all tasks (only one in database)
cat > "$input/read095.in" << \EOF











EOF
cat > "$output/read095.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: task 2	State: IN PROGRESS	Priority: VERY LOW	Type: ANALYSIS
	Start	Estimated: empty	Real: 21/12/2014
	End  	Estimated: 24/12/2014	Real: empty
	Time 	Estimated: -1	Real: 12
	Description: empty
	Notes: my personal notes
EOF
./launch-test.sh 'git task -r' 'read095'

./clean-db.sh
cat > "test-data.sql" << \EOF
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (2,'same name','IN PROGRESS',null,'my personal notes',null,'24/12/2014','21/12/2014',null,'VERY LOW','ANALYSIS',12,null);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (4,'same name','REJECTED',null,null,null,'27/12/2014',null,null,'URGENT','DEVELOPMENT',29,20);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (5,'task 5','REJECTED','my brief desc',null,'30/12/2014','21/12/2014',null,null,'VERY HIGH','CONFIGURATION',null,null);
.quit
EOF
./insert-data.sh
# TEST 96 --- read096 --- Search tasks by name (multiple tasks with same name)
cat > "$input/read096.in" << \EOF

same name









EOF
cat > "$output/read096.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: Tasks found
2 | Name: same name	State: IN PROGRESS	Priority: VERY LOW	Type: ANALYSIS
	Start	Estimated: empty	Real: 21/12/2014
	End  	Estimated: 24/12/2014	Real: empty
	Time 	Estimated: -1	Real: 12
	Description: empty
	Notes: my personal notes
4 | Name: same name	State: REJECTED	Priority: URGENT	Type: DEVELOPMENT
	Start	Estimated: empty	Real: empty
	End  	Estimated: 27/12/2014	Real: empty
	Time 	Estimated: 20	Real: 29
	Description: empty
	Notes: empty
EOF
./launch-test.sh 'git task -r' 'read096'


./clean-db.sh
# TEST 97 --- read097 --- Search all tasks (empty database)
cat > "$input/read097.in" << \EOF











EOF
cat > "$output/read097.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read097'

# TEST 98 --- read098 --- Search task by valid state (empty database) (minus)
cat > "$input/read098.in" << \EOF


new








EOF
cat > "$output/read098.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read098'

# TEST 99 --- read099 --- Search task by valid state (empty database) (mayus)
cat > "$input/read099.in" << \EOF


NEW








EOF
cat > "$output/read099.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read099'

# TEST 100 --- read100 --- Search task by valid state (empty database) (mixed [mayus | minus])
cat > "$input/read100.in" << \EOF


nEW








EOF
cat > "$output/read100.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read100'

# TEST 101 --- read101 --- Search task by valid priority (empty database) (minus)
cat > "$input/read101.in" << \EOF







high



EOF
cat > "$output/read101.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read101'

# TEST 102 --- read102 --- Search task by valid priority (empty database) (mayus)
cat > "$input/read102.in" << \EOF







HIGH



EOF
cat > "$output/read102.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read102'

# TEST 103 --- read103 --- Search task by valid priority (empty database) (mixed [mayus | minus])
cat > "$input/read103.in" << \EOF







hIgH



EOF
cat > "$output/read103.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read103'

# TEST 104 --- read104 --- Search task by valid type (empty database) (mixed [mayus | minus])
cat > "$input/read104.in" << \EOF








tEsT


EOF
cat > "$output/read104.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read104'

# TEST 105 --- read105 --- Search task by valid type (empty database) (mayus)
cat > "$input/read105.in" << \EOF








TEST


EOF
cat > "$output/read105.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read105'

# TEST 106 --- read106 --- Search task by valid type (empty database) (minus)
cat > "$input/read106.in" << \EOF








test


EOF
cat > "$output/read106.out" << \EOF
All filters are by equality
task id: task name: task state: task estimated start date: task estimated end date: task real start date: task real end date: task priority: task type: task estimated time: task real time: No task matching
EOF
./launch-test.sh 'git task -r' 'read106'

rm test-data.sql
rm insert-data.sh
