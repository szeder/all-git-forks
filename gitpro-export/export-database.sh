#!/bin/bash

echo "exporting tasks..."

final_name="gitpro-db.csv"

command="sqlite3 ../.git/gitpro.db -batch -csv"

cat > "temp-file" << \EOF
.output temp1
SELECT * FROM GP_TAREA;
.quit
EOF

eval "$command < temp-file"

cat > "temp-file" << \EOF
.output temp2
SELECT * FROM GP_USUARIO;
.quit
EOF

eval "$command < temp-file"

cat > "temp-file" << \EOF
.output temp3
SELECT * FROM GP_ASIGNACIONES;
.quit
EOF

eval "$command < temp-file"

#csv separator zone after task section and users section
# tasks
# break
# users
# break
# assignations
cat > "break" << \EOF
break
EOF


cat "temp1" "break" "temp2" "break" "temp3" > "$final_name"

rm "temp1" "temp2" "temp3" "break" "temp-file" 
