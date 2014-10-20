#!/bin/bash

# Create sql script to clean up database
cat > task-clean.sql << \EOF
DELETE FROM GP_TAREA;
DELETE FROM GP_ARCHIVO;
DELETE FROM GP_ASOCIACIONES;
DELETE FROM GP_ASIGNACIONES;
DELETE FROM GP_ARCHIVO;
DELETE FROM GP_ROL WHERE NOMBRE_ROL='ALL';
DELETE FROM GP_USUARIO WHERE NOMBRE_ROL_USUARIO='ALL';
.quit
EOF

# Launch clean action
sqlite3 ../../.git/gitpro.db -batch < task-clean.sql

rm task-clean.sql

# Clean exit with status 0
exit 0
