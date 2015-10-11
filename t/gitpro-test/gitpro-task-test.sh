#!/bin/bash

./before-env.sh

echo '***********************************'
echo '   Starting task creation tests'
echo '***********************************'
./gp-tcreate.sh
echo '***********************************'
echo '   Starting task reading tests'
echo '***********************************'
./gp-tread.sh
echo '***********************************'
echo 'Starting assist task reading tests'
echo '***********************************'
./gp-tread-assist.sh
echo '***********************************'
echo '   Starting task deletion tests'
echo '***********************************'
./gp-tdelete.sh
echo '***********************************'
echo 'Starting assist task delete tests'
echo '***********************************'
./gp-tdelete-assist.sh
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
echo '***********************************'
echo 'Starting show types/prior/state tests'
echo '***********************************'
./gp-tshow.sh
echo '***********************************'
echo '  Starting show pending tasks tests'
echo '***********************************'
./gp-tpending.sh
echo '***********************************'
echo '    Starting switch tasks tests'
echo '***********************************'
./gp-tswitch.sh
echo '***********************************'
echo '    Starting stat tasks tests'
echo '***********************************'
./gp-tstat.sh
echo '***********************************'
echo '    Starting export tasks tests'
echo '***********************************'
./gp-texport.sh
echo '***********************************'
echo '    Starting import tasks tests'
echo '***********************************'
./gp-timport.sh
echo '***********************************'
echo '   Starting rolecheck task tests'
echo '***********************************'
./gp-trolecheck.sh

./after-env.sh
