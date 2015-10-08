#!/bin/bash

# Import constants
source constants.sh

# Create sql script to clean up database
cat > task-clean.sql << \EOF
DELETE FROM GP_TAREA;
DELETE FROM GP_ARCHIVO;
DELETE FROM GP_ASOCIACIONES;
DELETE FROM GP_ASIGNACIONES;
DELETE FROM GP_ARCHIVO;
DELETE FROM GP_TIME_LOG;
.quit
EOF

# Launch clean action
sqlite3 $TEST_DB -batch < task-clean.sql

rm task-clean.sql

# Clean exit with status 0
exit 0
