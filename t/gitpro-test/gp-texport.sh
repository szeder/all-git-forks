#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	TASK EXPORT TESTS
###########################
echo "testing: git task --export"

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

INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol,consultar_tarea,asignar_tarea,actualizar_tarea,asociar_archivos,borrar_tarea,crear_tarea,borrar_rol,actualizar_rol) 
values('NONE',0,0,0,0,0,0,0,0,0,0);
UPDATE GP_USUARIO SET nombre_rol_usuario='NONE' where nombre_usuario='usertest';

INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) VALUES ('usertest',1);
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) VALUES ('usertest',3);
.quit
EOF

chmod +x insert-data.sh

./clean-db.sh
./insert-data.sh

# TEST 1 --- export001 --- Export tasks, asignations and users (error)
cat > "$input/export001.in" << \EOF
EOF
cat > "$output/export001.out" << \EOF
Output format not specified
EOF
./launch-test.sh 'git task --export' 'export001'

# TEST 2 --- export002 --- Export tasks, asignations and users (inexistent format)
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
xml
EOF
./launch-test.sh 'git task --export --out inexistent' 'export002'

# TEST 3 --- export003 --- Export tasks, asignations and users (valid output format)
cat > "$input/export003.in" << \EOF
EOF
cat > "$output/export003.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
+ Output xml generated
EOF
./launch-test.sh 'git task --export --out xml' 'export003'

./clean-db.sh
