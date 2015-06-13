#!/bin/bash

input="test-files"

###########################
# STD TO GANTTPROJECT TESTS
###########################
echo "testing: std-to-ganttproject"

###########################################
#Test 001 : Tasks, users and assignations
###########################################
cat > "expected" <<\EOF
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
	<resources>
		<resource id="426" name="pepe" />
	</resources>
	<allocations>
		<allocation resource-id="426" task-id="1" />
	</allocations>
	<taskdisplaycolumns>
		<displaycolumn property-id="state" order="3" width="20" visible="true" />
		<displaycolumn property-id="prior" order="4" width="20" visible="true" />
		<displaycolumn property-id="type" order="5" width="20" visible="true" />
	</taskdisplaycolumns>
</project>
EOF

./../compiled/std-to-ganttproject test-files/std-to-xxx001
./check.sh "std-to-ganttproject001" "ganttproject.gan" "expected"

###########################################
#Test 002 : Tasks
###########################################
cat > "expected" <<\EOF
<?xml version="1.0" encoding="UTF-8"?>
<project name="Exported GitPro Project" >
	<tasks>
		<taskproperties>
			<taskproperty id="prior" name="Priority" type="custom" valuetype="text" />
			<taskproperty id="type" name="Type" type="custom" valuetype="text" />
			<taskproperty id="state" name="State" type="custom" valuetype="text" />
		</taskproperties>
		<task id="1" name="nombre tarea" >
			<customproperty taskproperty-id="prior" value="alta" />
			<customproperty taskproperty-id="type" value="desarrollo" />
			<customproperty taskproperty-id="state" value="nueva" />
		</task>
	</tasks>
	<resources>
	</resources>
	<allocations>
	</allocations>
	<taskdisplaycolumns>
		<displaycolumn property-id="state" order="3" width="20" visible="true" />
		<displaycolumn property-id="prior" order="4" width="20" visible="true" />
		<displaycolumn property-id="type" order="5" width="20" visible="true" />
	</taskdisplaycolumns>
</project>
EOF

./../compiled/std-to-ganttproject test-files/std-to-xxx002
./check.sh "std-to-ganttproject002" "ganttproject.gan" "expected"

###########################################
#Test 003 : Users
###########################################
cat > "expected" <<\EOF
<?xml version="1.0" encoding="UTF-8"?>
<project name="Exported GitPro Project" >
	<tasks>
		<taskproperties>
			<taskproperty id="prior" name="Priority" type="custom" valuetype="text" />
			<taskproperty id="type" name="Type" type="custom" valuetype="text" />
			<taskproperty id="state" name="State" type="custom" valuetype="text" />
		</taskproperties>
	</tasks>
	<resources>
		<resource id="426" name="pepe" />
		<resource id="430" name="juan" />
	</resources>
	<allocations>
	</allocations>
	<taskdisplaycolumns>
		<displaycolumn property-id="state" order="3" width="20" visible="true" />
		<displaycolumn property-id="prior" order="4" width="20" visible="true" />
		<displaycolumn property-id="type" order="5" width="20" visible="true" />
	</taskdisplaycolumns>
</project>
EOF

./../compiled/std-to-ganttproject test-files/std-to-xxx003
./check.sh "std-to-ganttproject003" "ganttproject.gan" "expected"

###########################################
#Test 004 : Tasks and users
###########################################
cat > "expected" <<\EOF
<?xml version="1.0" encoding="UTF-8"?>
<project name="Exported GitPro Project" >
	<tasks>
		<taskproperties>
			<taskproperty id="prior" name="Priority" type="custom" valuetype="text" />
			<taskproperty id="type" name="Type" type="custom" valuetype="text" />
			<taskproperty id="state" name="State" type="custom" valuetype="text" />
		</taskproperties>
		<task id="1" name="nombre tarea" >
			<customproperty taskproperty-id="prior" value="alta" />
			<customproperty taskproperty-id="type" value="desarrollo" />
			<customproperty taskproperty-id="state" value="nueva" />
		</task>
	</tasks>
	<resources>
		<resource id="426" name="pepe" />
		<resource id="430" name="juan" />
	</resources>
	<allocations>
	</allocations>
	<taskdisplaycolumns>
		<displaycolumn property-id="state" order="3" width="20" visible="true" />
		<displaycolumn property-id="prior" order="4" width="20" visible="true" />
		<displaycolumn property-id="type" order="5" width="20" visible="true" />
	</taskdisplaycolumns>
</project>
EOF

./../compiled/std-to-ganttproject test-files/std-to-xxx004
./check.sh "std-to-ganttproject004" "ganttproject.gan" "expected"
