%{
#include "std-api/standarization-API.h"

int data = NONE;
int tvalue = -1;
int uvalue = -1;
int avalue = -1;

%}

%option noyywrap

intro [\n]

cadena [^<>\n]

%%

(?i:"<Tasks>")			{ data=TASK; }
(?i:"</Tasks>")			{ data=NONE; }
(?i:"<Resources>")		{ data = USER; }
(?i:"</Resources")		{ data = NONE; }
(?i:"<Assignments>") 	{ data = ASIG; }
(?i:"</Assignments>")	{ data = NONE; }

(?i:"<Task>")			{ tvalue=NONE;}
(?i:"</Task>")			{ tvalue=NONE;}
(?i:"<Resource>")		{ uvalue=NONE;}
(?i:"</Resource>")		{ uvalue=NONE;}
(?i:"<Assignment>")		{ avalue=NONE;}
(?i:"</Assignment>")	{ avalue=NONE;}

(?i:"<ID>")				{ tvalue = ID; }
(?i:"</ID>")			{ tvalue = NONE; }

(?i:"<Name>")			{ tvalue = NAME; uvalue=UNAME; }
(?i:"</Name>")			{ tvalue = NONE; uvalue=NONE; }

(?i:"<Start>")			{ tvalue = EST_INI; }
(?i:"</Start>")			{ tvalue = NONE; }

(?i:"<Finish>")			{ tvalue = EST_END; }
(?i:"</Finish>")		{ tvalue = NONE; }

(?i:"<ManualStart>")	{ tvalue = INI; }
(?i:"</ManualStart>")	{ tvalue = NONE; }

(?i:"<ManualFinish>")	{ tvalue = END; }
(?i:"</ManualFinish>")	{ tvalue = NONE; }

(?i:"<Duration>")		{ tvalue = EST_TIME; }
(?i:"</Duration>")		{ tvalue = NONE; }

(?i:"<ManualDuration>") { tvalue = TIME; }
(?i:"</ManualDuration>") { tvalue = NONE; }

(?i:"<TaskUID>") 		{ avalue = ATID; }
(?i:"</TaskUID>") 		{ avalue = NONE; }

(?i:"<ResourceUID>") 	{ avalue = AUNAME; }
(?i:"</ResourceUID>") 	{ avalue = NONE; }

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
