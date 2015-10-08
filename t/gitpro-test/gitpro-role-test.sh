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

echo 'Your username has been changed to run this tests...'
echo 'Use git config --global to update your username'
echo 'Do you want to restore now? (y/n) (default=n)'

read OPT
if [ "$OPT" == "y" ]; then
	echo 'Type your username: '
	read NAME
	eval "git config --global user.name $NAME"
fi

./after-env.sh
