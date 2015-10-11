#!/bin/bash

source constants.sh

###########################
# 	TASK SWITCH TESTS
###########################
echo "testing: git task --switch"


# TEST 1 --- switch001 --- Activate one task with no active task yet
cat > "$input/switch001.in" << \EOF
EOF
cat > "$output/switch001.out" << \EOF
+ Activating task...
Task 1 activated...
EOF
./launch-test.sh 'git task --switch 1' 'switch001'

# TEST 2 --- switch002 --- Trying to switch an inexistent task with one activated (negative)
cat > "$input/switch002.in" << \EOF
EOF
cat > "$output/switch002.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task --switch -2' 'switch002'

# TEST 3 --- switch003 --- Trying to switch an inexistent task with one activated
cat > "$input/switch003.in" << \EOF
EOF
cat > "$output/switch003.out" << \EOF
Task you're trying to activate / deactivate / change doesn't exists
EOF
./launch-test.sh 'git task --switch 12' 'switch003'

# TEST 4 --- switch004 --- Deactivate one task previously activated
cat > "$input/switch004.in" << \EOF
EOF
cat > "$output/switch004.out" << \EOF
- Deactivate task...
+ Tasks updated successfully
Task 1 deactivated ( 0.000000 hours spent )
EOF
./launch-test.sh 'git task --switch 1' 'switch004'

# TEST 5 --- switch005 --- Activate one task after deactivate other one
cat > "$input/switch005.in" << \EOF
EOF
cat > "$output/switch005.out" << \EOF
+ Activating task...
Task 2 activated...
EOF
./launch-test.sh 'git task --switch 2' 'switch005'

# TEST 6 --- switch006 --- Activate other task (we spect deactivation of previos activated task and activation of new task)
cat > "$input/switch006.in" << \EOF
EOF
cat > "$output/switch006.out" << \EOF
Deactivate previous task and activate actual...
+ Tasks updated successfully
Task 2 deactivated ( 0.000000 hours spent )
Task 3 activated...
EOF
./launch-test.sh 'git task --switch 3' 'switch006'

# TEST 7 --- switch007 --- Deactivate one task previously switched
cat > "$input/switch007.in" << \EOF
EOF
cat > "$output/switch007.out" << \EOF
- Deactivate task...
+ Tasks updated successfully
Task 3 deactivated ( 0.000000 hours spent )
EOF
./launch-test.sh 'git task --switch 3' 'switch007'


# TEST 8 --- switch008 --- Trying to switch an inexistent task with no active task
cat > "$input/switch008.in" << \EOF
EOF
cat > "$output/switch008.out" << \EOF
Task you're trying to activate / deactivate / change doesn't exists
EOF
./launch-test.sh 'git task --switch 10' 'switch008'


# TEST 9 --- switch009 --- Trying to switch an inexistent task with no active task (negative)
cat > "$input/switch009.in" << \EOF
EOF
cat > "$output/switch009.out" << \EOF
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task --switch -1' 'switch009'
