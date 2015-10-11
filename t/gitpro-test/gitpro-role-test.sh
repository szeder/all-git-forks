#!/bin/bash

source constants.sh

./before-env.sh

echo '***********************************'
echo '   Starting role creation tests'
echo '***********************************'
./gp-rcreate.sh
echo '***********************************'
echo '   Starting role reading tests'
echo '***********************************'
./gp-rread.sh
echo '***********************************'
echo '   Starting role deletion tests'
echo '***********************************'
./gp-rdelete.sh
echo '***********************************'
echo '   Starting role update tests'
echo '***********************************'
./gp-rupdate.sh
echo '***********************************'
echo '   Starting role assign tests'
echo '***********************************'
./gp-rassign.sh

./after-env.sh
