%{
#include "std-api/standarization-API.h"

/* Actual reading value for task data (starts with task id) */
int tvalue = ID;
/* Actual reading value for user data (starts with user id) */
int uvalue = UNAME;
/* Actual reading value for asignation data (starts with task id) */
int avalue = AUNAME;

/* Actuar reading data (starts with tasks) */
int data = TASK;

%}

%option noyywrap

intro [\n]

cadena [^,\n]

%%

","					{ switch(data){
							case TASK: tvalue++; break;
							case USER: uvalue++; break;
							case ASIG: avalue++; break;
					   }
					}

"break"				{ data++; }

{intro}				{ 
						//reset to initial param of each type of data
						tvalue = ID;
						uvalue = UNAME;
						avalue = AUNAME;
						std_task();
						std_user();
						std_asig();
					}

{cadena}+ 			{  
						switch(data){
							case TASK:
								switch(tvalue){
									case ID: set_id(yytext); break;
									case NAME: set_name(yytext); break;
									case STATE: set_state(yytext); break;
									case DESC: set_desc(yytext); break;
									case NOTES: set_notes(yytext); break;
									case EST_INI: set_est_ini(yytext); break; 
									case EST_END: set_est_end(yytext); break;
									case INI: set_ini(yytext); break;
									case END: set_end(yytext);break;
									case PRIOR: set_priority(yytext); break;
									case TYPE: set_type(yytext); break;
									case TIME: set_time(yytext); break;
									case EST_TIME: set_est_time(yytext); break;
								}
								break;
							case USER:
								switch(uvalue){
									case UNAME: set_uname(yytext); break;
									case UROLE: set_urole(yytext); break;
								}
								break;
							case ASIG:
								switch(avalue){
									case AUNAME: set_auname(yytext);  break;
									case ATID: set_atid(yytext); break;
								}
								break;
						}
					}

.					{ /* vacio */ }

%%

main(){
	yylex();
}
