%{
#include "std-api/standarization-API.h"

int pos = 0;

%}

%option noyywrap

intro [\n]

cadena [^,\n]

%%

","					{ pos++; }

{intro}				{ /* vacio */ }

{cadena}+ 			{  switch(pos){
							case 0: std_id(yytext); break;
							case 1: std_name(yytext); break;
							case 2: std_state(yytext); break;
							case 3: std_desc(yytext); break;
							case 4: std_notes(yytext); break;
							case 5: std_est_ini(yytext); break; 
							case 6: std_est_end(yytext); break;
							case 7: std_ini(yytext); break;
							case 8: std_end(yytext);break;
							case 9: std_priority(yytext); break;
							case 10: std_type(yytext); break;
							case 11: std_est_time(yytext); break;
							case 12: std_time(yytext); pos=0; break;
						}
					}

.					{ /* vacio */ }

%%

main(){
	yylex();
}
