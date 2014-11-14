#!/bin/bash

input="test_input"
output="test_output"

###########################
# 	TASK SHOW TESTS
###########################

echo "testing: git task --show-types"

# TEST  1 --- showtypes001 --- Show available types
cat > "$input/showtypes001.in" << \EOF
EOF
cat > "$output/showtypes001.out" << \EOF
Available task types:
> ANALYSIS
> DESIGN
> MANAGEMENT
> QUALITY
> DEVELOPMENT
> TEST
> MAINTENANCE
> SUPPORT
> CONFIGURATION
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
> REJECTED
> RESOLVED
EOF
./launch-test.sh 'git task --show-states' 'showstates001'

echo "testing: git task --show-priorities"

# TEST  1 --- showpriorities001 --- Show available priorities
cat > "$input/showpriorities001.in" << \EOF
EOF
cat > "$output/showpriorities001.out" << \EOF
Available task priorities:
> VERY LOW
> LOW
> MINOR
> MAJOR
> HIGH
> VERY HIGH
> URGENT
> INMEDIATE
> CRITICAL
> BLOCKER
EOF
./launch-test.sh 'git task --show-priorities' 'showpriorities001'
