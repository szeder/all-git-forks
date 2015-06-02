#!/bin/sh

rm -rf .git
git init
echo "content file 1" > file1; git add file1; git commit -m "Ajout du fichier1"
echo "content file 2" > file2; git add file2; git commit -m "Ajout du fichier2"
echo "content file 3" > file3; git add file3; git commit -m "Ajout du fichier3"
echo "content file 4" > file4; git add file4; git commit -m "Ajout du fichier4"
echo "content file 5" > file5; git add file5; git commit -m "Ajout du fichier5"
echo "content file 6" > file6; git add file6; git commit -m "Ajout du fichier6"
