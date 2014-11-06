#!/bin/bash

# Create sql script to clean up database
cat > user-clean.sql << \EOF
DELETE FROM GP_USUARIO WHERE NOMBRE_ROL_USUARIO='EXAMPLE';
DELETE FROM GP_ROL WHERE NOMBRE_ROL='EXAMPLE';
.quit
EOF

# Launch clean action
sqlite3 ../../.git/gitpro.db -batch < user-clean.sql

rm user-clean.sql

# Clean exit with status 0
exit 0
