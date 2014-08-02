#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "generic_list.h"
#include "db_constants.h"

/* Local lists that will be filled with data */
role_list roles;
task_list tasks;
user_list users;
file_list files;
asoc_list asocs;
asig_list asigs;

/* Local list that will be returned to abstract from data structure */
generic_list generic;

/*******************/
/* QUERY FUNCTIONS */
/*******************/

/*	Name		: exec_nonquery
	Parameters	: sql query string
	Return		: 1 if ok or 0 in other case
	Used for	: sql sentences like INSERT, UPDATE or DELETE */
int exec_nonquery(const char *sql);

/*	Name		: exec_query
	Parameters	: sql query string
	Return		: generic list filled with requested data in query.
				 remember to free this list using dealloc function in this API
	Used for	: sql sentences like SELECT  */
generic_list exec_query(const char *sql);

/* 	Name		: exec_int_scalarquery
	Parameters	: sql query string
	Return		: scalar int resulting in query, if query has no scalar result returns 0
	Used for	: sql sentences with COUNT, AVG and similar operators */
int exec_int_scalarquery(const char *sql);

/*	Name		: exec_float_scalarquery
	Parameters	: sql query string
	Return		: scalar float resulting in query, if query has no scalar result return 0.0
	Used for	: sql sentences with COUNT, AVG and similar operators */
float exec_float_scalarquery(const char *sql);

/*******************/
/*    FREE DATA	   */
/*******************/

/*	Name 		: dealloc
	Parameters	: generic list to be freed
	Return		: nothing
	Used for	: free allocated memory */
void dealloc(generic_list list);


/*******************/
/*    SHOW DATA    */
/*******************/

/*	Name		: show_xxxxx
	Parameters	: xxxx list to show info
	Return		: nothing
	Used for	: debugging purposes. prints in standard output info of given list */
void show_roles(role_list list);
void show_tasks(task_list list);
void show_users(user_list list);
void show_files(file_list list);
void show_asigs(asig_list list);
void show_asocs(asoc_list list);


/*******************/
/*    SQL HELPER   */
/*******************/
/* Use with db_constants.h
   This functions only construct sql queries to make a faster development. 
   This functions does not make any data validation so, an string have to be entered like
	'string' or \"string\" depending on database engine 
   You can try using format_string given in this API. */

/* 	Name		: format_string
	Parameters	: original string to apply format 
	Return		: original string between (') symbol. remember to free this pointer
				 with free function in stdlib.h
	Used for	: use strings in 'sql helper' functions
	Example		: format_string("hola") would return "'hola'" */
char *format_string(char *orig);

/* 	Name		: format_number
	Parameters	: number to convert
	Return		: string representation of given number. remember to free this pointer
				 with free function in stdlib.h
	Used for	: use numbers in 'sql helper functions'
	Example		: format_number(39) would return "39" */
char *format_number(int number);

/*	Name		: new_task_id
	Parameters	: nothing
	Return		: next task id available to new task
	Used for	: automatically obtain new unique id for new task insert */
int new_task_id();


/*	Name		: construct_query
	Parameters	: fields to select part, from part (see from function), where part (see where
				 function), group by part (see group_by function) and order by part (see
				 order_by function). where, group by and order by parts can be NULL 
				 and not apply
	Return		: string with query to execute with one of given functions in this API.
				 (see exec_query) or NULL if at least one parameter (except where, group by
				 or order by) is null.
				 remember to free this pointer using free function in stdlib.h
	Used for	: make a basic sql query faster
	Notes		: to fields parameter is required that last element is null or 0.
				 from,where,group_by and order by is automatically freed
	Example		: construct_query({ALL,0},from(TASK_TABLE),NULL,NULL,NULL) would return
				 'SELECT * FROM GP_TAREA' */
char *construct_query(char *fields[],char *from,char *where,char *group_by,char *order_by);

/*	Name		: construct_insert
	Parameters	: table to insert data, fields to insert and data to be inserted
	Return		: string with query to execute with one of given functions in this API
				 (see exec_nonquery) or NULL if at least one parameter is null.
 				 remember to free this pointer using free function in stdlib.h
	Used for	: make a basic sql insert faster 
	Notes		: to fields and values parameteters requires that last element is null or 0
	Example		: construct_insert(TASK_TABLE,{TASK_NAME,TASK_STATE,0},{"'name1'","'new'",0}) 
				 would return 'INSERT INTO GP_TAREA(NOMBRE,ESTADO) VALUES ('name1','new') */
char *construct_insert(char *table,char *fields[],char *values[]);

/*	Name		: construct_delete
	Parameters	: table name and where condition that can be null
	Return		: string with query to execute with one of given functions in this API
				 (see exec_nonquery) or null if table name is null. remember to free
				 this pointer using free function in stdlib.h
	Used for	: make a basic sql delete faster
	Notes		: if where part is null query returned will delete all table rows 
	Example		: construct_delete(TASK_TABLE,where(cond(TASK_ID,">","5"))) would return
				 'DELETE FROM GP_TAREA WHERE id>5' */
char *construct_delete(char *table,char *where);

/* 	Name		: construct_update
	Parameters	: table name, asignation array (see asig function) 
				 and where condition that can be null (see where function).
	Return		: string with query to execute with one of given functions in this API
				 (see exec_nonquery) or null if at least one parameter (except where) is null.
				 remember to free this pointer using free function in stdlib.h
	Used for	: make a basic sql update faster 
	Notes		: to field asig is required that last element is null or 0 
	Example		: construct_update(TASK_TABLE,{asig(TASK_NAME,"'nombre'"),
							asig(TASK_STATE,"'en progreso'"),0},
							where(cond(TASK_ID,"=","3"))) 
				 would return 'UPDATE GP_TAREA SET nombre_tarea='nombre',estado='en progreso'
				 WHERE id=3' */
char *construct_update(char *table,char *asig[],char *where);

/* 	Name		: asig
	Parameters	: field to asign and new value
	Return		: string with asignation or null if at least one parameter is null
	Used for	: update asignations (see construct_update) 
	Note		: if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h */
char *asig(char *field,char *value);

/* 	Name		: from
	Parameters	: table name
	Return		: from part to use in construct_query function in this API. if table name
				 is null, returns null 
	Note		: if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h 
	Example		: from(TASK_TABLE) would return 'FROM GP_TAREA' */
char *from(char *table_name);

/* 	Name		: where
	Parameters	: where conditions (see cond, _and_, _or_ functions) 
	Return		: where part to use in construct_query function in this API or null 
				 if conditions is null 
	Note		: if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h 
	Example		: where(conds) would return 'WHERE conds' */
char *where(char *conds);

/*	Name		: group_by
	Parameters	: fields for what data will be grouped
	Return		: group by part to use in construct_query function in this API or null
				 if fields is null 
	Notes		: to field fields is required that last element is null or 0.
				  if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h 
	Example		: group_by({TASK_STATE,0}) would return 'GROUP BY ESTADO' */
char *group_by(char *fields[]);

/*	Name		: order_by
	Parameters	: fields for what data will be order and flag to change between natural
				 order or inverse order
	Return		: order by part to use in construct_query function in this API or null
				 if fields is null 
	Notes		: to field fields is required that last element is null or 0.
				  if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h 
	Example		: order_by({TASK_STATE,0},1) would return 'ORDER BY ESTADO DESC' */
char *order_by(char *fields[],int inverse);

/*	Name		: cond
	Parameters	: field to compare, comparator and field to be compared
	Return		: condition to use in where function in this API or null if some parameter
				 is null
	Note		: if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h 
	Example		: cond(TASK_ID,"=","8") would return 'ID=8' */
char *cond(char *field1,char *comp,char *field2);
	
/*	Name		: _and_
	Parameters	: two conditions to apply and
	Return		: and of two given conditions or null if at least one of given conditions
				 is null. it can be used in where function as other condition
	Note		: if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h 
	Example		: _and_(cond(...),cond(...)) would return '... AND ...' */
char *_and_(char *cond1,char *cond2);

/*	Name		: _or_
	Parameters	: two conditions to apply or
	Return		: or of two given conditions or null if at least one of given conditions 
				 is null. it can be used in where function as other condition
	Note		: if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h 
	Example		: _or_(cond(...),cond(...)) would return '... OR ...' */
char *_or_(char *cond1,char *cond2);

/*	Name		: count
	Parameters	: chosen field
	Return		: count expression to use as field in construct query for scalar results (f.e)
	Used for	: number of elements
	Note		: function remember to free pointer with free function in stdlib.h 
	Example		: count(TASK_STATE) would return 'COUNT(ESTADO)'*/
char *count(char *field);

/*	Name		: avg
	Parameters	: chosen field
	Return		: avg expression to use as field in construct query for scalar results (f.e)
				 or null if field is null
	Used for	: average value
	Note		: if returned value not used in construct_xxxxx function remember to 
				 free pointer with free function in stdlib.h */
char *avg(char *field);

/*	Name		: min
	Parameters	: chosen field
	Return		: min expression to use as field in construct query for scalar results (f.e)
				 or null if field is null
	Used for	: minimum value
	Note		: function remember to free pointer with free function in stdlib.h */
char *min(char *field);

/*	Name		: max
	Parameters	: chosen field
	Return		: max expression to use as field in construct query for scalar results (f.e)
				 or null if field is null
	Used for	: maximum value
	Note		: function remember to free pointer with free function in stdlib.h */
char *max(char *field);

/*	Name		: sum
	Parameters	: chosen field
	Return		: sum expression to use as field in construct query for scalar results (f.e)
				 or null if field is null
	Used for	: sum of values
	Note		: function remember to free pointer with free function in stdlib.h */
char *sum(char *field);

/*	Name		: lower
	Parameters	: chosen field
	Return		: lower expression to use as field in construct query for scalar results (f.e)
				 or null if field is null
	Used for	: string to lower case
	Note		: function remember to free pointer with free function in stdlib.h */
char *lower(char *field);

/*	Name		: upper
	Parameters	: chosen field
	Return		: upper expression to use as field in construct query for scalar results (f.e)
				 or null if field is null
	Used for	: string to upper case
	Note		: function remember to free pointer with free function in stdlib.h */
char *upper(char *field);

