#!/bin/bash

source constants.sh

###########################
# 	TASK EXPORT TESTS
###########################
echo "testing: git task --export"

# TEST 1 --- export001 --- Export with no arguments
cat > "$input/export001.in" << \EOF
EOF
cat > "$output/export001.out" << \EOF
Output format not specified
EOF
./launch-test.sh 'git task --export' 'export001'

# TEST 2 --- export002 --- Export to inexistent output format
cat > "$input/export002.in" << \EOF
EOF
cat > "$output/export002.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
inexistent output format not available
available output formats are:
csv
ganttproject
json
EOF
./launch-test.sh 'git task --export --out inexistent' 'export002'

# TEST 3 --- export003 --- Export to csv
cat > "$input/export003.in" << \EOF
EOF
cat > "$output/export003.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
* Input file: gitpro-db.csv
* Input format: csv
+ Generating file in csv format
+ Output csv generated
EOF
./launch-test.sh 'git task --export --out csv' 'export003'
rm "../../gitpro-export/csv-task.csv"

# TEST 4 --- export004 --- Export to ganttproject
cat > "$input/export004.in" << \EOF
EOF
cat > "$output/export004.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
* Input file: gitpro-db.csv
* Input format: csv
+ Generating file in ganttproject format
+ Output ganttproject generated
EOF
./launch-test.sh 'git task --export --out ganttproject' 'export004'
rm "../../gitpro-export/ganttproject.gan"

# TEST 5 --- export005 --- Export to json
cat > "$input/export005.in" << \EOF
EOF
cat > "$output/export005.out" << \EOF
+ Exporting tasks...OK
+ Exporting users...OK
+ Exporting assignations...OK
+ Data exported to gitpro-db.csv
* Input file: gitpro-db.csv
* Input format: csv
+ Generating file in json format
+ Output json generated
EOF
./launch-test.sh 'git task --export --out json' 'export005'
rm "../../gitpro-export/json-task.json"
