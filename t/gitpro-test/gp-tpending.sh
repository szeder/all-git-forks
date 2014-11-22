#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	TASK ASSIGN TESTS
###########################
echo "testing: git task -a"

# Insert previous data into tasks to run following test
cat > "test-data.sql" << \EOF
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol,consultar_tarea,asignar_tarea,actualizar_tarea,asociar_archivos,borrar_tarea,crear_tarea,borrar_rol,actualizar_rol) 
values('EXAMPLE',0,0,1,1,1,1,1,1,1,1);
INSERT INTO GP_USUARIO(nombre_usuario,nombre_rol_usuario) values ('user1','EXAMPLE');
INSERT INTO GP_USUARIO(nombre_usuario,nombre_rol_usuario) values ('user2','EXAMPLE');
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

INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('usertest',1);
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('usertest',5);
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('usertest',3);
.quit
EOF

chmod +x insert-data.sh

### ASSIGN ADD FILES TESTS

./clean-db.sh
./insert-data.sh
# TEST  1 --- pending001 --- check out pending tasks
cat > "$input/pending001.in" << \EOF
EOF
cat > "$output/pending001.out" << \EOF
These are pending tasks for usertest :
* 1 | NEW | task 1
* 3 | IN PROGRESS | task 3
EOF
./launch-test.sh 'git task --pending' 'pending001'

cat > "test-data.sql" << \EOF
DELETE FROM GP_ASIGNACIONES;
.quit
EOF

./insert-data.sh
# TEST  2 --- pending002 --- no pending tasks
cat > "$input/pending002.in" << \EOF
EOF
cat > "$output/pending002.out" << \EOF
You haven't assigned tasks yet...
EOF
./launch-test.sh 'git task --pending' 'pending002'
