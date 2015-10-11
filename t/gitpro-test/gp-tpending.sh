#!/bin/bash

source constants.sh

###########################
# 	TASK PENDING TASK TESTS
###########################
echo "testing: git task --pending"

### ASSIGN ADD FILES TESTS

# TEST  1 --- pending001 --- check out pending tasks
cat > "$input/pending001.in" << \EOF
EOF
cat > "$output/pending001.out" << \EOF
These are pending tasks for usertest :
* 1 | NEW | task 1
* 3 | IN PROGRESS | task 3
EOF
./launch-test.sh 'git task --pending' 'pending001'

# TEST  2 --- pending002 --- no pending tasks
cat > "$input/pending002.in" << \EOF
EOF
cat > "$output/pending002.out" << \EOF
You haven't assigned tasks yet...
EOF
./launch-test.sh 'git task --pending' 'pending002' 'def-user1'
