#!/bin/bash

#Analyzers constants
A_CSV="csv-to-std"
A_GANTTPROJECT="ganttproject-to-std"

#TRANSLATORS
T_CSV="std-to-csv"
T_GANTTPROJECT="std-to-ganttproject"
T_JSON="std-to-json"

#FORMATS NAME
CSV="csv"
GANTTPROJECT="ganttproject"
JSON="json"

#COMPILE ANALYZERS
echo "#########################"
echo "   compiling analyzers"
echo "#########################"
make std NAME=$A_CSV FORMAT=$CSV
make clean
make std NAME=$A_GANTTPROJECT FORMAT=$GANTTPROJECT
make clean

#COMPILE TRANSLATORS
echo "#########################"
echo "  compiling translators"
echo "#########################"
make dest NAME=$T_CSV FORMAT=$CSV
make clean
make dest NAME=$T_GANTTPROJECT FORMAT=$GANTTPROJECT
make clean
make dest NAME=$T_JSON FORMAT=$JSON
make clean

mv *-to-std compiled/
mv std-to-* compiled/
