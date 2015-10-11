#!/bin/bash

source constants.sh

insert_sql="insert-data.sql"

if [ "$1" == 'def-user1' ]; then
git config --global user.name user1
else
git config --global user.name usertest
fi

if [ "$1" != "empty" ]; then

if [ "$1" == "only-one" ]; then
cat > $insert_sql << \EOF
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (2,'task 2','IN PROGRESS',null,'my personal notes',null,'24/12/2014','21/12/2014',null,'VERY LOW','ANALYSIS',12,null);
.quit
EOF

sqlite3 $TEST_DB -batch < $insert_sql

rm $insert_sql

else

cat > $insert_sql << \EOF
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
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (6,'same name','IN PROGRESS',null,'my personal notes',null,'24/12/2014','21/12/2014',null,'VERY LOW','ANALYSIS',12,null);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (7,'same name','REJECTED',null,null,null,'27/12/2014',null,null,'URGENT','DEVELOPMENT',29,20);
INSERT INTO GP_TAREA(id,nombre_tarea,estado_tarea,descripcion,notas,fecha_inicio_estimada,fecha_final_estimada,fecha_inicio_real,fecha_final_real,prioridad_tarea,tipo_tarea,tiempo_real,tiempo_estimado) 
values (8,'task 8','REJECTED','my brief desc',null,'30/12/2014','21/12/2014',null,null,'VERY HIGH','CONFIGURATION',null,null);

.quit
EOF

sqlite3 $TEST_DB -batch < $insert_sql

rm $insert_sql
fi
fi

cat > $insert_sql << \EOF
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol) values ('TEST_A',1,1);
INSERT INTO GP_ROL(nombre_rol) values ('TEST_B');
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol,consultar_tarea,asignar_tarea,actualizar_tarea,asociar_archivos,borrar_tarea,crear_tarea,borrar_rol,actualizar_rol) 
values('EXAMPLE',1,1,1,1,1,1,1,1,1,1);

INSERT INTO GP_ESTADO values('NEW');
INSERT INTO GP_ESTADO values('IN PROGRESS');
INSERT INTO GP_ESTADO values('RESOLVED');
INSERT INTO GP_ESTADO values('REJECTED');

INSERT INTO GP_PRIOR values('BLOCKER');
INSERT INTO GP_PRIOR values('URGENT');
INSERT INTO GP_PRIOR values('VERY HIGH');
INSERT INTO GP_PRIOR values('HIGH');
INSERT INTO GP_PRIOR values('MAJOR');
INSERT INTO GP_PRIOR values('LOW');
INSERT INTO GP_PRIOR values('VERY LOW');

INSERT INTO GP_TIPO values('SUPPORT');
INSERT INTO GP_TIPO values('DEVELOPMENT');
INSERT INTO GP_TIPO values('ANALYSIS');
INSERT INTO GP_TIPO values('MANAGEMENT');
INSERT INTO GP_TIPO values('CONFIGURATION');
INSERT INTO GP_TIPO values('TEST');

INSERT INTO GP_USUARIO (nombre_usuario,nombre_rol_usuario) values ('usertest','EXAMPLE');
INSERT INTO GP_USUARIO(nombre_usuario,nombre_rol_usuario) values ('user1','EXAMPLE');
INSERT INTO GP_USUARIO(nombre_usuario,nombre_rol_usuario) values ('user2','EXAMPLE');

.quit
EOF

sqlite3 $TEST_DB -batch < $insert_sql

rm $insert_sql


if [ "$1" != "no-asignations" ]; then
cat > $insert_sql << \EOF
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('usertest',1);
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('usertest',5);
INSERT INTO GP_ASIGNACIONES(nombre_usuario_asig,id_asig) values ('usertest',3);

.quit
EOF

sqlite3 $TEST_DB -batch < $insert_sql

rm $insert_sql
fi

if [ "$1" == "test-asociations" ]; then
cat > $insert_sql << \EOF
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values('f1','ruta1');
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest1','rutaA');
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest2','rutaB');
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest3','rutaC');
INSERT INTO GP_ARCHIVO(nombre_archivo,ruta_archivo) values ('mytest3','rutaCbis');
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('ruta1',1);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('rutaA',1);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('rutaB',2);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('rutaB',4);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('rutaA',4);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('rutaC',3);
INSERT INTO GP_ASOCIACIONES(ruta_archivo_asoc,id_asoc) values ('rutaCbis',3);
.quit
EOF

sqlite3 $TEST_DB -batch < $insert_sql
rm $insert_sql
fi

if [ "$1" == "no-permission" ]; then
cat > $insert_sql << \EOF
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol,consultar_tarea,asignar_tarea,actualizar_tarea,asociar_archivos,borrar_tarea,crear_tarea,borrar_rol,actualizar_rol) 
values('NONE',0,0,0,0,0,0,0,0,0,0);
UPDATE GP_USUARIO SET nombre_rol_usuario='NONE' where nombre_usuario='usertest';
.quit
EOF
sqlite3 $TEST_DB -batch < $insert_sql
rm $insert_sql
fi

if [ "$1" == "no-roles" ]; then
cat > $insert_sql << \EOF
DELETE FROM GP_ROL WHERE nombre_rol='NONE';
DELETE FROM GP_ROL WHERE nombre_rol='ALL';
DELETE FROM GP_USUARIO WHERE nombre_usuario='usertest';
.quit
EOF
sqlite3 $TEST_DB -batch < $insert_sql
rm $insert_sql
fi

#################################
