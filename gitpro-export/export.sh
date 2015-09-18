#!/bin/bash

# If number of arguments less then 3; print usage and exit
if [ $# -lt 3 ]; then
    printf "Usage: $0 <in format> <out format> <input file>\n"
    exit 1
fi

folder="compiled/"

available_in_formats="in-formats"
available_out_formats="out-formats"

in_format="$1"           # The application (from command arg)
out_format="$2"	   # The file name to test (.in and .out had to have same name)

in_file="$3"

post_in="-to-std"
pre_out="std-to-"

analyzer="$folder$in_format$post_in"
translator="$folder$pre_out$out_format"

# Validate analyzer
if [ ! -f "$analyzer" ]; then
printf "%s input format not available\n" "$in_format"
printf "available input formats are:\n"
cat "$folder$available_in_formats"
exit;
fi

#Validate translator
if [ ! -f "$translator" ]; then
printf "%s output format not available\n" "$out_format"
printf "available output formats are:\n"
cat "$folder$available_out_formats"
exit;
fi

#Validate input file
if [ ! -f "$in_file" ]; then
printf "%s input file not exist\n" "$in_file"
exit;
fi

printf "* Input file: %s\n" "$in_file"
printf "* Input format: %s\n" "$in_format"

printf "+ Generating file in %s format\n" "$out_format"

./$analyzer < "$in_file" 
		
./$translator "standard-file"

printf "+ Output %s generated\n" "$out_format"


if [ -f "gitpro-db.csv" ]; then
	rm "gitpro-db.csv"
fi

if [ -f "standard-file" ]; then
	rm "standard-file"
fi
