#!/bin/bash

source constants.sh

delete_sql="delete-data.sql"

# Delete previous data if exists
#################################
cat > $delete_sql <<\EOF
DELETE FROM GP_ROL;
DELETE FROM GP_USUARIO;
DELETE FROM GP_ASOCIACIONES;
DELETE FROM GP_ASIGNACIONES;
DELETE FROM GP_TAREA;
DELETE FROM GP_ARCHIVO;
DELETE FROM GP_TIME_LOG;
.quit
EOF

sqlite3 $TEST_DB -batch < $delete_sql

rm $delete_sql
#################################
