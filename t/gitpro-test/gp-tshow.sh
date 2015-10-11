#!/bin/bash

source constants.sh

###########################
# 	TASK SHOW TESTS
###########################

echo "testing: git task --show-types"

# TEST  1 --- showtypes001 --- Show available types
cat > "$input/showtypes001.in" << \EOF
EOF
cat > "$output/showtypes001.out" << \EOF
Available task types:
> SUPPORT
> DEVELOPMENT
> ANALYSIS
> MANAGEMENT
> CONFIGURATION
> TEST
EOF
./launch-test.sh 'git task --show-types' 'showtypes001'

echo "testing: git task --show-types"

# TEST  1 --- showstates001 --- Show available states
cat > "$input/showstates001.in" << \EOF
EOF
cat > "$output/showstates001.out" << \EOF
Available task states:
> NEW
> IN PROGRESS
> RESOLVED
> REJECTED
EOF
./launch-test.sh 'git task --show-states' 'showstates001'

echo "testing: git task --show-priorities"

# TEST  1 --- showpriorities001 --- Show available priorities
cat > "$input/showpriorities001.in" << \EOF
EOF
cat > "$output/showpriorities001.out" << \EOF
Available task priorities:
> BLOCKER
> URGENT
> VERY HIGH
> HIGH
> MAJOR
> LOW
> VERY LOW
EOF
./launch-test.sh 'git task --show-priorities' 'showpriorities001'
