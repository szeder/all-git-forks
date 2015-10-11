#!/bin/bash

source constants.sh

###########################
# 	TASK LINK TESTS
###########################
echo "testing: git task -l"

cat > "test_file1" << \EOF
file 1
EOF

cat > "test_file2" << \EOF
file 2
EOF

cat > "test_file3" << \EOF
file 3
EOF

mkdir asoc_test
cd asoc_test
cat > "test_file3" << \EOF
file 3 bis
EOF
cd ..

### LINK ADD FILES TESTS

# TEST  1 --- link001 --- links a new successfull basic task
cat > "$input/link001.in" << \EOF
EOF
cat > "$output/link001.out" << \EOF
+ Selected file '/t/gitpro-test/test_file1'
+ Asociated file '/t/gitpro-test/test_file1'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="test_file1"' 'link001'


# TEST  2 --- link002 --- links a new successfull basic task (multiple paths)
cat > "$input/link002.in" << \EOF
0
EOF
cat > "$output/link002.out" << \EOF
Has found more than one path for file or folder 'test_file3'
Select one [0 - 1] and press ENTER
0 | /t/gitpro-test/asoc_test/test_file3
1 | /t/gitpro-test/test_file3
+ Selected file '/t/gitpro-test/asoc_test/test_file3'
+ Asociated file '/t/gitpro-test/asoc_test/test_file3'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="test_file3"' 'link002'

# TEST  3 --- link003 --- links a new successfull basic task to a folder
cat > "$input/link003.in" << \EOF
EOF
cat > "$output/link003.out" << \EOF
+ Selected file '/t/gitpro-test/asoc_test'
+ Asociated file '/t/gitpro-test/asoc_test'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="asoc_test"' 'link003'

# TEST  4 --- link004 --- links a new successfull basic task (multiple files at time)
cat > "$input/link004.in" << \EOF
EOF
cat > "$output/link004.out" << \EOF
+ Selected file '/t/gitpro-test/test_file1'
+ Asociated file '/t/gitpro-test/test_file1'
+ Selected file '/t/gitpro-test/test_file2'
+ Asociated file '/t/gitpro-test/test_file2'
EOF
./launch-test.sh 'git task -l -i 1 --file --add="test_file1,test_file2"' 'link004'

# TEST  5 --- link005 --- links a new successfull basic task (invalid task id)
cat > "$input/link005.in" << \EOF
EOF
cat > "$output/link005.out" << \EOF
Task you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 15 --file --add="test_file2"' 'link005'

# TEST  6 --- link006 --- links a new successfull basic task (invalid task id) (negative)
cat > "$input/link006.in" << \EOF
EOF
cat > "$output/link006.out" << \EOF
Task you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i -5 --file --add="test_file2"' 'link006'

# TEST  7 --- link007 --- links a new successfull basic task (invalid task id) (zero)
cat > "$input/link007.in" << \EOF
EOF
cat > "$output/link007.out" << \EOF
Task you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 0 --file --add="test_file2"' 'link007'

# TEST  8 --- link008 --- links a new successfull basic task (invalid name)
cat > "$input/link008.in" << \EOF
EOF
cat > "$output/link008.out" << \EOF
inexistent does not exists...
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -l -i 2 --file --add="inexistent"' 'link008'

# TEST  9 --- link009 --- links a new successfull basic task (invalid name and other valid)
cat > "$input/link009.in" << \EOF
EOF
cat > "$output/link009.out" << \EOF
inexistent does not exists...
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -l -i 2 --file --add="inexistent,test_file1"' 'link009'

# TEST 10 --- link010 --- links a new successfull basic task (valid name and other invalid)
cat > "$input/link010.in" << \EOF
EOF
cat > "$output/link010.out" << \EOF
inexistent does not exists...
Incorrect data. Check it all and try again
EOF
./launch-test.sh 'git task -l -i 2 --file --add="test_file1,inexistent"' 'link010'

### LINK RM FILES TESTS

# TEST  11 --- link011 --- unlinks a new successfull basic task
cat > "$input/link011.in" << \EOF
EOF
cat > "$output/link011.out" << \EOF
+ Selected file 'rutaA'
- Deasociated file 'rutaA'
EOF
./launch-test.sh 'git task -l -i 1 --file --rm="mytest1"' 'link011' 'test-asociations'

# TEST 12 --- link012 --- unlinks a new successfull basic task (multiple paths)
cat > "$input/link012.in" << \EOF
0
EOF
cat > "$output/link012.out" << \EOF
Has found more than one path for file or folder 'mytest3'
Select one [0 - 1] and press ENTER
0 | rutaC
1 | rutaCbis
+ Selected file 'rutaC'
- Deasociated file 'rutaC'
EOF
./launch-test.sh 'git task -l -i 3 --file --rm="mytest3"' 'link012' 'test-asociations'

# TEST  13 --- link013 --- unlinks a new successfull basic task (multiple files at time)
cat > "$input/link013.in" << \EOF
EOF
cat > "$output/link013.out" << \EOF
+ Selected file 'rutaB'
- Deasociated file 'rutaB'
+ Selected file 'rutaA'
- Deasociated file 'rutaA'
EOF
./launch-test.sh 'git task -l -i 4 --file --rm="mytest2,mytest1"' 'link013' 'test-asociations'

# TEST  14 --- link014 --- unlinks a new successfull basic task (invalid task id)
cat > "$input/link014.in" << \EOF
EOF
cat > "$output/link014.out" << \EOF
Task you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 15 --file --rm="mytest2"' 'link014' 'test-asociations'

# TEST  15 --- link015 --- unlinks a new successfull basic task (invalid task id) (negative)
cat > "$input/link015.in" << \EOF
EOF
cat > "$output/link015.out" << \EOF
Task you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i -5 --file --rm="mytest2"' 'link015' 'test-asociations'

# TEST  16 --- link016 --- unlinks a new successfull basic task (invalid task id) (zero)
cat > "$input/link016.in" << \EOF
EOF
cat > "$output/link016.out" << \EOF
Task you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 0 --file --rm="mytest2"' 'link016' 'test-asociations'

# TEST  17 --- link017 --- unlinks a new successfull basic task (invalid name)
cat > "$input/link017.in" << \EOF
EOF
cat > "$output/link017.out" << \EOF
File or Folder you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 2 --file --rm="inexistent"' 'link017' 'test-asociations'

# TEST  18 --- link018 --- unlinks a new successfull basic task (invalid name and other valid)
cat > "$input/link018.in" << \EOF
EOF
cat > "$output/link018.out" << \EOF
File or Folder you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 2 --file --rm="inexistent,mytest1"' 'link018' 'test-asociations'

# TEST 19 --- link019 --- unlinks a new successfull basic task (valid name and other invalid)
cat > "$input/link019.in" << \EOF
EOF
cat > "$output/link019.out" << \EOF
File or Folder you're trying to link / unlink doesn't exists
EOF
./launch-test.sh 'git task -l -i 2 --file --rm="mytest1,inexistent"' 'link019' 'test-asociations'


rm test_file1 test_file2 test_file3
rm -rf asoc_test
