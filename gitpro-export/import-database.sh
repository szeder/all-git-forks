#!/bin/bash

if [ $# -lt 1 ]; then
    printf "Usage: $0 <input file>\n"
    exit 1
fi

command="sqlite3 ../.git/gitpro.db"
sqlite_import_file="sqlite-import"

file_name=$1

READ_TASK=1
READ_USER=2
READ_ASIG=3

section=$READ_TASK

#Separate tasks, users and assignations to import in appropiate table
#into gitpro database
while read line; do
	if [ "$line" == "break" ]; then
		((section=$section+1))
	else
		if [ "$section" == "$READ_TASK" ]; then
			echo "$line" >> "tasks"
		fi
		if [ "$section" == "$READ_USER" ]; then
			echo "$line" >> "users"
		fi
		if [ "$section" == "$READ_ASIG" ]; then
			echo "$line" >> "asigs"
		fi		
	fi
done < "$file_name"

printf "+ Importing tasks..."

cat > "$sqlite_import_file" << \EOF
.separator ','
.import tasks GP_TAREA
.quit
EOF

eval "$command < $sqlite_import_file"

printf "OK\n"

printf "+ Importing users..."

cat > "$sqlite_import_file" << \EOF
.separator ','
.import users GP_USUARIO
.quit
EOF

eval "$command < $sqlite_import_file"

printf "OK\n"

printf "+ Importing assignations..."

cat > "$sqlite_import_file" << \EOF
.separator ','
.import asigs GP_ASIGNACIONES
.quit
EOF

eval "$command < $sqlite_import_file"

printf "OK\n"

printf "+ Data imported to gitpro database\n"

rm "tasks" "users" "asigs" "sqlite-import" "csv-task.csv"
