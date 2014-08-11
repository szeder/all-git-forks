gcc -o sqlite3_test test.c ../../gitpro_api/*.c ../../gitpro_api/*.h -lsqlite3
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test0.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test1.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test2.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test3.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test4.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test5.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test6.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test7.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test8.txt
rm gitpro.db
sqlite3 gitpro.db < sql_test.sql
./sqlite3_test > test9.txt
rm gitpro.db

