#!/bin/bash

source constants.sh

###########################
# 	TASK IMPORT TESTS
###########################
echo "testing: git task --import"


# TEST 1 --- import001 --- Import with no input file and without input format
cat > "$input/import001.in" << \EOF
EOF
cat > "$output/import001.out" << \EOF
Input format not specified
EOF
./launch-test.sh 'git task --import' 'import001' 

# TEST 2 --- import002 --- Import with input file (inexistent) and without input format
cat > "$input/import002.in" << \EOF
EOF
cat > "$output/import002.out" << \EOF
Input format not specified
EOF
./launch-test.sh 'git task --import --input-file inexistent' 'import002' 

# TEST 3 --- import003 --- Import without input file and with input format (inexistent)
cat > "$input/import003.in" << \EOF
EOF
cat > "$output/import003.out" << \EOF
Input file not specified
EOF
./launch-test.sh 'git task --import --in inexistent' 'import003'

# TEST 4 --- import004 --- Import from csv
cat > "$input/test-csv.csv" << \EOF
1,"task 1",NEW,"my desc","my notes",20/12/2014,21/12/2014,,,HIGH,TEST,12,14
2,"task 2","IN PROGRESS",,"my personal notes",,24/12/2014,21/12/2014,,"VERY LOW",ANALYSIS,12,
3,"task 3","IN PROGRESS",,,,26/12/2014,,28/12/2014,MAJOR,MANAGEMENT,,18
4,"task 4",REJECTED,,,,27/12/2014,,,URGENT,DEVELOPMENT,29,20
5,"task 5",REJECTED,"my brief desc",,30/12/2014,21/12/2014,,,"VERY HIGH",CONFIGURATION,,
break
break
EOF
cat > "$input/import004.in" << \EOF
EOF
cat > "$output/import004.out" << \EOF
* Input file: ../t/gitpro-test/test_input/test-csv.csv
* Input format: csv
+ Generating file in csv format
+ Output csv generated
+ Importing tasks...OK
+ Importing users...OK
+ Importing assignations...OK
+ Data imported to gitpro database
EOF
./launch-test.sh 'git task --import --in csv --input-file ../t/gitpro-test/test_input/test-csv.csv' 'import004' 'empty'
rm "$input/test-csv.csv"

# TEST 5 --- import005 --- Import from ganttproject
cat > "$input/test-ganttproject.gan" << \EOF
<?xml version="1.0" encoding="UTF-8"?>
<project name="Exported GitPro Project" >
	<tasks>
		<taskproperties>
			<taskproperty id="prior" name="Priority" type="custom" valuetype="text" />
			<taskproperty id="type" name="Type" type="custom" valuetype="text" />
			<taskproperty id="state" name="State" type="custom" valuetype="text" />
		</taskproperties>
		<task id="1" name="nombre tarea" start="2010-11-15" duration="2" complete="100" >
			<customproperty taskproperty-id="prior" value="alta" />
			<customproperty taskproperty-id="type" value="desarrollo" />
			<customproperty taskproperty-id="state" value="nueva" />
		</task>
		<task id="2" name="otra tarea" >
			<customproperty taskproperty-id="state" value="terminada" />
		</task>
	</tasks>
	<taskdisplaycolumns>
		<displaycolumn property-id="state" order="3" width="20" visible="true" />
		<displaycolumn property-id="prior" order="4" width="20" visible="true" />
		<displaycolumn property-id="type" order="5" width="20" visible="true" />
	</taskdisplaycolumns>
</project>
EOF
cat > "$input/import005.in" << \EOF
EOF
cat > "$output/import005.out" << \EOF
* Input file: ../t/gitpro-test/test_input/test-ganttproject.gan
* Input format: ganttproject
+ Generating file in csv format
+ Output csv generated
+ Importing tasks...OK
+ Importing users...OK
+ Importing assignations...OK
+ Data imported to gitpro database
EOF
./launch-test.sh 'git task --import --in ganttproject --input-file ../t/gitpro-test/test_input/test-ganttproject.gan' 'import005' 'empty'
rm "$input/test-ganttproject.gan"
