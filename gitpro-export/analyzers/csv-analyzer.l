%{
#include "std-api/standarization-API.h"

/* Actual reading value for task data (starts with task id) */
int tvalue = ID;
/* Actual reading value for user data (starts with user id) */
int uvalue = UNAME;
/* Actual reading value for asignation data (starts with task id) */
int avalue = ATID;

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

{intro}				{ /* vacio */ }

{cadena}+ 			{  
						switch(data){
							case TASK:
								switch(tvalue){
									case ID: std_id(yytext); break;
									case NAME: std_name(yytext); break;
									case STATE: std_state(yytext); break;
									case DESC: std_desc(yytext); break;
									case NOTES: std_notes(yytext); break;
									case EST_INI: std_est_ini(yytext); break; 
									case EST_END: std_est_end(yytext); break;
									case INI: std_ini(yytext); break;
									case END: std_end(yytext);break;
									case PRIOR: std_priority(yytext); break;
									case TYPE: std_type(yytext); break;
									case EST_TIME: std_est_time(yytext); break;
									case TIME: std_time(yytext); tvalue=ID; break;
								}
								break;
							case USER:
								switch(uvalue){
									case UNAME: std_uname(yytext); break;
									case UROLE: break;
								}
								break;
							case ASIG:
								switch(avalue){
									case ATID: std_atid(yytext); break;
									case AUNAME: std_auname(yytext); avalue=ATID; break;
								}
								break;
						}
					}

.					{ /* vacio */ }

%%

main(){
	yylex();
}
