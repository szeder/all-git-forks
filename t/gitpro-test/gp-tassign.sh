#!/bin/bash

source constants.sh

###########################
# 	TASK ASSIGN TESTS
###########################
echo "testing: git task -a"

### ASSIGN ADD FILES TESTS

# TEST  1 --- assign001 --- assigns a new successfull basic task
cat > "$input/assign001.in" << \EOF
EOF
cat > "$output/assign001.out" << \EOF
** Asignations to task 1 modified **
+ Asigned user user1
EOF
./launch-test.sh 'git task -a -i 1 --user --add="user1"' 'assign001'

# TEST  2 --- assign002 --- assigns a new successfull basic task (multiple users)
cat > "$input/assign002.in" << \EOF
EOF
cat > "$output/assign002.out" << \EOF
** Asignations to task 1 modified **
+ Asigned user user1
+ Asigned user user2
EOF
./launch-test.sh 'git task -a -i 1 --user --add="user1,user2"' 'assign002'

# TEST  3 --- assign003 --- assigns a new successfull basic task (invalid id)
cat > "$input/assign003.in" << \EOF
EOF
cat > "$output/assign003.out" << \EOF
Task you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i 16 --user --add="user1"' 'assign003'

# TEST  4 --- assign004 --- assigns a new successfull basic task (invalid id) (zero)
cat > "$input/assign004.in" << \EOF
EOF
cat > "$output/assign004.out" << \EOF
Task you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i 0 --user --add="user1"' 'assign004'

# TEST  5 --- assign005 --- assigns a new successfull basic task (invalid id) (negative)
cat > "$input/assign005.in" << \EOF
EOF
cat > "$output/assign005.out" << \EOF
Task you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i -5 --user --add="user1"' 'assign005'

# TEST  6 --- assign006 --- assigns a new successfull basic task (invalid user)
cat > "$input/assign006.in" << \EOF
EOF
cat > "$output/assign006.out" << \EOF
User you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i 2 --user --add="inexistent"' 'assign006'

#### RM ASSIGN TESTS

# TODO: fix this test (add assignations to deasignate)
cat > "test-data.sql" << \EOF
DELETE FROM GP_ASIGNACIONES;
DELETE FROM GP_ROL WHERE NOMBRE_ROL = 'EXAMPLE';
DELETE FROM GP_USUARIO WHERE NOMBRE_USUARIO = 'user1';
DELETE FROM GP_USUARIO WHERE NOMBRE_USUARIO = 'user2';
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol,consultar_tarea,asignar_tarea,actualizar_tarea,asociar_archivos,borrar_tarea,crear_tarea,borrar_rol,actualizar_rol) 
values('EXAMPLE',0,0,1,1,1,1,1,1,1,1);
INSERT INTO GP_USUARIO(nombre_usuario,nombre_rol_usuario) values ('user1','EXAMPLE');
INSERT INTO GP_USUARIO(nombre_usuario,nombre_rol_usuario) values ('user2','EXAMPLE');
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('user1',1);
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('user2',1);
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('user1',3);
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

# TEST  7 --- assign007 --- deassigns a new successfull basic task (invalid deasignation)
cat > "$input/assign007.in" << \EOF
EOF
cat > "$output/assign007.out" << \EOF
** Asignations to task 4 modified **
- Deasigned user user1
EOF
./launch-test.sh 'git task -a -i 4 --user --rm="user1"' 'assign007'

# TEST  8 --- assign008 --- deassigns a new successfull basic task (1 user)
cat > "$input/assign008.in" << \EOF
EOF
cat > "$output/assign008.out" << \EOF
** Asignations to task 2 modified **
- Deasigned user user1
EOF
./launch-test.sh 'git task -a -i 2 --user --rm="user1"' 'assign008'

# TEST  9 --- assign009 --- deassigns a new successfull basic task (2 user)
cat > "$input/assign009.in" << \EOF
EOF
cat > "$output/assign009.out" << \EOF
** Asignations to task 1 modified **
- Deasigned user user1
- Deasigned user user2
EOF
./launch-test.sh 'git task -a -i 1 --user --rm="user1,user2"' 'assign009'

# TEST  10 --- assign010 --- deassigns a new successfull basic task (invalid id)
cat > "$input/assign010.in" << \EOF
EOF
cat > "$output/assign010.out" << \EOF
Task you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i 10 --user --rm="user1"' 'assign010'

# TEST  11 --- assign011 --- deassigns a new successfull basic task (invalid id) (zero)
cat > "$input/assign011.in" << \EOF
EOF
cat > "$output/assign011.out" << \EOF
Task you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i 0 --user --rm="user1"' 'assign011'

# TEST  12 --- assign012 --- deassigns a new successfull basic task (invalid id) (negative)
cat > "$input/assign012.in" << \EOF
EOF
cat > "$output/assign012.out" << \EOF
Task you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i -3 --user --rm="user1"' 'assign012'

# TEST  13 --- assign013 --- deassigns a new successfull basic task (invalid user)
cat > "$input/assign013.in" << \EOF
EOF
cat > "$output/assign013.out" << \EOF
User you're trying to assign / deassign doesn't exists
EOF
./launch-test.sh 'git task -a -i 2 --user --rm="inexistent"' 'assign013'

## COMBINING ADD AND RM

# TEST  14 --- assign014 --- assign and deassign a new successfull basic task 
cat > "$input/assign014.in" << \EOF
EOF
cat > "$output/assign014.out" << \EOF
** Asignations to task 2 modified **
+ Asigned user user2
- Deasigned user user1
EOF
./launch-test.sh 'git task -a -i 2 --user --add="user2" --rm="user1"' 'assign014'
