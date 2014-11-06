#!/bin/bash

git config --global user.name usertest

cat > "test-data.sql" << \EOF
INSERT INTO GP_ROL(nombre_rol,crear_rol,asignar_rol,consultar_tarea,asignar_tarea,actualizar_tarea,asociar_archivos,borrar_tarea,crear_tarea,borrar_rol,actualizar_rol) 
values('EXAMPLE',1,1,1,1,1,1,1,1,1,1);
INSERT INTO GP_USUARIO (nombre_usuario,nombre_rol_usuario) values ('usertest','EXAMPLE');
.quit
EOF

chmod +x insert-data.sh
./insert-data.sh
