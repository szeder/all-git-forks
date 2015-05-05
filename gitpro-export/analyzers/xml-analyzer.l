%{
#include "std-api/standarization-API.h"

int isTask = 0;
int param = -1;

%}

%option noyywrap

intro [\n]

cadena [^<>\n]

%%

(?i:"<Task>")			{ /* Cambiamos el flag isTask a 1, indicando asi que estamos en una tarea */
							isTask=1; }

(?i:"</Task>")			{ /* Cambiamos el flag isTask a 0, indicando que ya se termino la informacion de la tarea
							 y param = -1 debido a que no hay ningun parametro activo en este momento */
							isTask=0;
						  param=-1; }

(?i:"<ID>")				{ /* Indicamos el parametro correspondiente para estandarizarlo cuando se detecte la cadena */
							param = 0; }
(?i:"</ID>")			{ /* Cuando se cierra el tag desactivamos el parametro indicando que no hay ninguno activo */
							param = -1; }

(?i:"<Name>")			{ param = 1; }
(?i:"</Name>")			{ param = -1; }

(?i:"<Start>")			{ param = 5; }
(?i:"</Start>")			{ param = -1; }

(?i:"<Finish>")			{ param = 6; }
(?i:"</Finish>")		{ param = -1; }

(?i:"<ManualStart>")	{ param = 7; }
(?i:"</ManualStart>")	{ param = -1; }

(?i:"<ManualFinish>")	{ param = 8; }
(?i:"</ManualFinish>")	{ param = -1; }

(?i:"<Duration>")		{ param = 11; }
(?i:"</Duration>")		{ param = -1; }

(?i:"<ManualDuration>") { param = 12; }
(?i:"</ManualDuration>") { param = -1; }

{intro}				{ /* vacio */ }

{cadena}+ 			{  switch(param){
							case 0: std_id(yytext); break;
							case 1: std_name(yytext); break;
							//case 2: std_state(yytext); break;
							//case 3: std_desc(yytext); break;
							//case 4: std_notes(yytext); break;
							case 5: std_est_ini(yytext); break; 
							case 6: std_est_end(yytext); break;
							case 7: std_ini(yytext); break;
							case 8: std_end(yytext);break;
							//case 9: std_priority(yytext); break;
							//case 10: std_type(yytext); break;
							case 11: std_est_time(yytext); break;
							case 12: std_time(yytext); break;
						}
					}

.					{ /* vacio */ }

%%

main(){
	yylex();
}
