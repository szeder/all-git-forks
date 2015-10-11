#!/bin/bash

source constants.sh

###########################
# 	TASK STAT TESTS
###########################
echo "testing: git task --stat"

# TEST 1 --- stat001 --- Stats with some tasks in database
cat > "$input/stat001.in" << \EOF
EOF
cat > "$output/stat001.out" << \EOF
*****************************************************
               Project Statistics
*****************************************************
Project task completion : 0.00 %
	Time including rejected tasks
		- Estimated time : 72.00 hours
		- Real time      : 94.00 hours
	Time without rejected tasks
		- Total estimated time : 32.00 hours
		- Total real time      : 36.00 hours
Total tasks: 8
	Task number by state
		NEW : 1 (12.50 %)
		IN PROGRESS : 3 (37.50 %)
		RESOLVED : 0 (0.00 %)
		REJECTED : 4 (50.00 %)
	Task number by priority
		BLOCKER : 0 (0.00 %)
		URGENT : 2 (25.00 %)
		VERY HIGH : 2 (25.00 %)
		HIGH : 1 (12.50 %)
		MAJOR : 1 (12.50 %)
		LOW : 0 (0.00 %)
		VERY LOW : 2 (25.00 %)
	Task number by type
		SUPPORT : 0 (0.00 %)
		DEVELOPMENT : 2 (25.00 %)
		ANALYSIS : 2 (25.00 %)
		MANAGEMENT : 1 (12.50 %)
		CONFIGURATION : 2 (25.00 %)
		TEST : 1 (12.50 %)
	Task assignations by user
		user1 : 0 (0.00 %)
		user2 : 0 (0.00 %)
		usertest : 3 (37.50 %)
	Time logged by user
		user1 : 0.00 hours
		user2 : 0.00 hours
		usertest : 0.00 hours
EOF
./launch-test.sh 'git task --stat' 'stat001'

# TEST 2 --- stat002 --- Stats without tasks in database
cat > "$input/stat002.in" << \EOF
EOF
cat > "$output/stat002.out" << \EOF
*****************************************************
               Project Statistics
*****************************************************
Total tasks: 0
EOF
./launch-test.sh 'git task --stat' 'stat002' 'empty'
