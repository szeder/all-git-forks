#!/bin/bash

final_name="gitpro-db.csv"

command="sqlite3 ../.git/gitpro.db -batch -csv"

cat > "temp-file" << \EOF
.output temp1
.mode list
SELECT * FROM GP_TAREA;
.quit
EOF

printf "+ Exporting tasks..."
eval "$command < temp-file"

cat > "temp-file" << \EOF
.output temp2
SELECT * FROM GP_USUARIO;
.quit
EOF

printf "OK\n"

printf "+ Exporting users..."
eval "$command < temp-file"

cat > "temp-file" << \EOF
.output temp3
SELECT * FROM GP_ASIGNACIONES;
.quit
EOF

printf "OK\n"

printf "+ Exporting assignations..."
eval "$command < temp-file"

printf "OK\n"

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

printf "+ Data exported to %s\n" "$final_name"

rm "temp1" "temp2" "temp3" "break" "temp-file" 
