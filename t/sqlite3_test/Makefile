CC=gcc
LIB=-lsqlite
PATH=../../gitpro_api/

all: create_db compile run drop_db

create_db:
	sqlite3 gitpro.db < sql_test.sql

compile:
	gcc -o sqlite3_test test.c ../../gitpro_api/*.c ../../gitpro_api/*.h -lsqlite3
	
run: 
	./sqlite3_test
	
drop_db:
	rm gitpro.db
	
clean: 
	rm sqlite3_test
	rm *~
