#!/bin/bash

v1="$1"
v2="$2"

db_path=".git/gitpro.db"

echo "Extracting data from $v1"
sqlite3 $v1 .dump > dump_v1

echo "Extracting data from $v2"
sqlite3 $v2 .dump > dump_v2

echo "Searching differences between files..."
diff dump_v1 dump_v2 > patch-diff

echo "Merging files..."
patch -p0 dump_v1 patch-diff > null

echo "Generating new merged database..."
sqlite3 new_db < dump_v1

echo "Cleaning conflicted files..."
rm dump_v1 dump_v2 patch-diff
rm $db_path

echo "Loading merged working database..."
cp new_db $db_path 

echo "Merge completed successfully!!"
