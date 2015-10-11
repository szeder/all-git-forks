#!/bin/bash

source constants.sh

# Create temporal folders to input and out data
#################################
mkdir $input
mkdir $output
#################################

# Remove all data in database
# Run here exceptionally
#################################
./after-test.sh
#################################
