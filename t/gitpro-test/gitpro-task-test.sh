#!/bin/bash

cat > "insert-data.sh" << \EOF
sqlite3 ../../.git/gitpro.db -batch < test-data.sql
exit 0
EOF

git config --global user.name usertest

cat > "test-data.sql" << \EOF
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol,consultar_tarea,asignar_tarea,actualizar_tarea,asociar_archivos,borrar_tarea,crear_tarea,borrar_rol,actualizar_rol) 
values('ALL',1,1,1,1,1,1,1,1,1,1);
INSERT INTO GP_USUARIO (nombre_usuario,nombre_rol_usuario) values ('usertest','ALL');
.quit
EOF
./insert-data.sh

echo '***********************************'
echo '   Starting task creation tests'
echo '***********************************'
./gp-tcreate.sh
./clean-db.sh
exit
echo '***********************************'
echo '   Starting task reading tests'
echo '***********************************'
./gp-tread.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task deletion tests'
echo '***********************************'
./gp-tdelete.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task update tests'
echo '***********************************'
./gp-tupdate.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task assign tests'
echo '***********************************'
./gp-tassign.sh
./clean-db.sh
echo '***********************************'
echo '   Starting task link tests'
echo '***********************************'
./gp-tlink.sh
./clean-db.sh
echo '***********************************'
echo '   Starting rolecheck task tests'
echo '***********************************'
./gp-trolecheck.sh
./clean-db.sh

echo 'Your username has been changed to run this tests...'
echo 'Use git config --global to update your username'

rm test-data.sql
rm insert-data.sh
