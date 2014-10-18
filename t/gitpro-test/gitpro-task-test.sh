#!/bin/bash

echo '***********************************'
echo '   Starting task creation tests'
echo '***********************************'
./gp-tcreate.sh
echo '***********************************'
echo '   Starting task reading tests'
echo '***********************************'
./gp-tread.sh
echo '***********************************'
echo '   Starting task deletion tests'
echo '***********************************'
./gp-tdelete.sh
echo '***********************************'
echo '   Starting task update tests'
echo '***********************************'
./gp-tupdate.sh
echo '***********************************'
echo '   Starting task assign tests'
echo '***********************************'
./gp-tassign.sh
echo '***********************************'
echo '   Starting task link tests'
echo '***********************************'
./gp-tlink.sh
