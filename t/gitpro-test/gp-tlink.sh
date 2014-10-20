#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	TASK LINK TESTS
###########################
echo "testing: git task -l"

# Insert previous data into tasks to run following test
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

cat > "f1" << \EOF
file 1
EOF

cat > "f2" << \EOF
file 2
EOF

cat > "f3" << \EOF
file 3
EOF

mkdir asoc_test
cd asoc_test
cat > "f3" << \EOF
file 3 bis
EOF
cd ..


chmod +x insert-data.sh

### LINK ADD FILES TESTS

./clean-db.sh
./insert-data.sh
# TEST  1 --- link001 --- links a new successfull basic task
cat > "$input/link001.in" << \EOF
EOF
cat > "$output/link001.out" << \EOF
0 | /t/gitpro-test/f1
+ Selected unique file
+ Asociated file '/t/gitpro-test/f1'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="f1"' 'link001'

./clean-db.sh
./insert-data.sh

# TEST  2 --- link002 --- links a new successfull basic task (multiple paths)
cat > "$input/link002.in" << \EOF
0
EOF
cat > "$output/link002.out" << \EOF
Has found more than one path for file or folder 'f3'
Select one [0 - 1] and press ENTER
0 | /t/gitpro-test/asoc_test/f3
1 | /t/gitpro-test/f3
+ Selected file '/t/gitpro-test/asoc_test/f3'
+ Asociated file '/t/gitpro-test/f3'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="f3"' 'link002'

./clean-db.sh
./insert-data.sh
# TEST  3 --- link003 --- links a new successfull basic task to a folder
cat > "$input/link003.in" << \EOF
EOF
cat > "$output/link003.out" << \EOF
0 | /t/gitpro-test/asoc_test
+ Selected unique file
+ Asociated file '/t/gitpro-test/asoc_folder'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="asoc_test"' 'link003'

./clean-db.sh
./insert-data.sh
# TEST  4 --- link004 --- links a new successfull basic task (multiple files at time)
cat > "$input/link004.in" << \EOF
EOF
cat > "$output/link004.out" << \EOF
0 | /t/gitpro-test/f1
+ Selected unique file
+ Asociated file '/t/gitpro-test/f1'
0 | /t/gitpro-test/f2
+ Selected unique file
+ Asociated file '/t/gitpro-test/f2'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="f1,f2"' 'link004'

./clean-db.sh
./insert-data.sh
# TEST  5 --- link005 --- links a new successfull basic task (invalid task id)
cat > "$input/link005.in" << \EOF
EOF
cat > "$output/link005.out" << \EOF
Task you're trying to link doesn't exists
EOF
./launch-test.sh 'git task -l -i 15 --file --add="f2"' 'link005'

./clean-db.sh
./insert-data.sh
# TEST  6 --- link006 --- links a new successfull basic task (invalid task id) (negative)
cat > "$input/link006.in" << \EOF
EOF
cat > "$output/link006.out" << \EOF
Task you're trying to link doesn't exists
EOF
./launch-test.sh 'git task -l -i -5 --file --add="f2"' 'link006'

./clean-db.sh
./insert-data.sh
# TEST  7 --- link007 --- links a new successfull basic task (invalid task id) (zero)
cat > "$input/link007.in" << \EOF
EOF
cat > "$output/link007.out" << \EOF
Task you're trying to link doesn't exists
EOF
./launch-test.sh 'git task -l -i 0 --file --add="f2"' 'link007'

./clean-db.sh
./insert-data.sh
# TEST  8 --- link008 --- links a new successfull basic task (invalid name)
cat > "$input/link008.in" << \EOF
EOF
cat > "$output/link008.out" << \EOF
inexistent does not exists...
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -l -i 2 --file --add="inexistent"' 'link008'

./clean-db.sh
./insert-data.sh
# TEST  9 --- link009 --- links a new successfull basic task (invalid name and other valid)
cat > "$input/link009.in" << \EOF
EOF
cat > "$output/link009.out" << \EOF
inexistent does not exists...
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -l -i 2 --file --add="inexistent,f1"' 'link009'

./clean-db.sh
./insert-data.sh
# TEST 10 --- link010 --- links a new successfull basic task (valid name and other invalid)
cat > "$input/link010.in" << \EOF
EOF
cat > "$output/link010.out" << \EOF
inexistent does not exists...
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -l -i 2 --file --add="f1,inexistent"' 'link010'

### LINK RM FILES TESTS

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
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest1','ruta1');
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest2','ruta2');
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest3','ruta3');
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest3','ruta3bis');
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('ruta1',1);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('ruta2',2);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('ruta2',4);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('ruta1',4);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('ruta3',3);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('ruta3bis',3);
.quit
EOF

./clean-db.sh
./insert-data.sh
# TEST  11 --- link011 --- unlinks a new successfull basic task
cat > "$input/link011.in" << \EOF
EOF
cat > "$output/link011.out" << \EOF
0 | ruta1
+ Selected unique file
- Deasociated file 'ruta1'
EOF
./launch-test.sh 'git task -l -i 1 --file --rm="mytest1"' 'link011'


./clean-db.sh
./insert-data.sh

# TEST 12 --- link012 --- unlinks a new successfull basic task (multiple paths)
cat > "$input/link012.in" << \EOF
0
EOF
cat > "$output/link012.out" << \EOF
Has found more than one path for file or folder 'mytest3'
Select one [0 - 1] and press ENTER
0 | ruta3
1 | ruta3bis
+ Selected file 'ruta3'
- Deasociated file 'ruta3'
EOF
./launch-test.sh 'git task -l -i 3 --file --rm="mytest3"' 'link012'

./clean-db.sh
./insert-data.sh
# TEST  13 --- link013 --- unlinks a new successfull basic task (multiple files at time)
cat > "$input/link013.in" << \EOF
EOF
cat > "$output/link013.out" << \EOF
0 | ruta2
+ Selected unique file
- Deasociated file 'ruta2'
0 | ruta1
+ Selected unique file
- Deasociated file 'ruta1'
EOF
./launch-test.sh 'git task -l -i 4 --file --rm="mytest2,mytest1"' 'link013'

./clean-db.sh
./insert-data.sh
# TEST  14 --- link014 --- unlinks a new successfull basic task (invalid task id)
cat > "$input/link014.in" << \EOF
EOF
cat > "$output/link014.out" << \EOF
Task you're trying to link doesn't exists
EOF
./launch-test.sh 'git task -l -i 15 --file --rm="mytest2"' 'link014'

./clean-db.sh
./insert-data.sh
# TEST  15 --- link015 --- unlinks a new successfull basic task (invalid task id) (negative)
cat > "$input/link015.in" << \EOF
EOF
cat > "$output/link015.out" << \EOF
Task you're trying to link doesn't exists
EOF
./launch-test.sh 'git task -l -i -5 --file --rm="mytest2"' 'link015'

./clean-db.sh
./insert-data.sh
# TEST  16 --- link016 --- unlinks a new successfull basic task (invalid task id) (zero)
cat > "$input/link016.in" << \EOF
EOF
cat > "$output/link016.out" << \EOF
Task you're trying to link doesn't exists
EOF
./launch-test.sh 'git task -l -i 0 --file --rm="mytest2"' 'link016'

./clean-db.sh
./insert-data.sh
# TEST  17 --- link017 --- unlinks a new successfull basic task (invalid name)
cat > "$input/link017.in" << \EOF
EOF
cat > "$output/link017.out" << \EOF
File or Folder you're trying to unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 2 --file --rm="inexistent"' 'link017'

./clean-db.sh
./insert-data.sh
# TEST  18 --- link018 --- unlinks a new successfull basic task (invalid name and other valid)
cat > "$input/link018.in" << \EOF
EOF
cat > "$output/link018.out" << \EOF
File or Folder you're trying to unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 2 --file --rm="inexistent,mytest1"' 'link018'

./clean-db.sh
./insert-data.sh
# TEST 19 --- link019 --- unlinks a new successfull basic task (valid name and other invalid)
cat > "$input/link019.in" << \EOF
EOF
cat > "$output/link019.out" << \EOF
File or Folder you're trying to unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 2 --file --rm="mytest1,inexistent"' 'link019'


rm -r asoc_test
rm f1 f2 f3
