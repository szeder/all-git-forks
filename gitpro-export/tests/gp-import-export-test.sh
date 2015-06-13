#!/bin/bash

echo "testing analyzers..."
./gp-csv-to-std.sh
./gp-ganttproject-to-std.sh
echo "analyzers testing complete"

echo "testing translators..."
./gp-std-to-csv.sh
./gp-std-to-ganttproject.sh
./gp-std-to-json.sh
echo "translators testing complete"

printf "\n\nNOTE: If this test have not run, execute 'compile-all.sh' in 'gitpro-export' folder\n"
