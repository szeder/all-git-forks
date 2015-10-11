#!/bin/bash

cd gitpro-test

source constants.sh

echo 'Running this tests will erase all your database data'
echo 'Do you want to backup your actual database? (y/n) (default=n)'
read backupOpt
if [ "$backupOpt" == "y" ]; then
	echo 'Give a name to backup database:'
	read backupName
	cp $TEST_DB $TEST_DB_PATH"/"$backupName
fi

#Run task command tests
./gitpro-task-test.sh

#Run role command tests
./gitpro-role-test.sh

echo 'All tests finished...'
echo 'Do you want to restore any backup database now? (y/n) (default=n)'
read restoreOpt

if [ "$restoreOpt" == "y" ]; then
	echo 'Enter your backup database name (will be in .db folder):'
	read restoreName
	echo 'Enter your git username:'
	read userName
	cp $TEST_DB_PATH"/"$restoreName $TEST_DB
	git config --global user.name $userName
	echo "Database $restoreName restored successfully and changed global user to $userName"
fi
