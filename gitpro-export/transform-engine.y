%{

#include <stdio.h>
#include "iface-translate.h"

/* Tratamiento de errores de la carga de datos */
void yyerror(char const *s){
	printf("PARSE_ERROR: %s\n",s);
}

%}

%error-verbose

%union {
	char *cad;
}

/* Terminales de la gramática */
%token <cad> FIELD
%token <char *> ID
%token <char *> NAME
%token <char *> STATE
%token <char *> DESCRIPTION
%token <char *> NOTES
%token <char *> EDATE_INI
%token <char *> EDATE_END
%token <char *> RDATE_INI
%token <char *> RDATE_END
%token <char *> PRIORITY
%token <char *> TYPE
%token <char *> E_TIME
%token <char *> R_TIME

/* No terminal inicial */
%type <char *> s

%type <char *> id
%type <char *> nombre
%type <char *> estado
%type <char *> descripcion
%type <char *> notas
%type <char *> est_ini
%type <char *> est_end
%type <char *> ini
%type <char *> end
%type <char *> prioridad
%type <char *> tipo
%type <char *> est_time
%type <char *> time

%type <char *> t

%type <char *> pretask
%type <char *> posttask

%type <char *> end_task_sect
%type <char *> start_task_sect

%type <char *> tarea


/* Definimos el simbolo inicial */
%start s

%%

s: start_task_sect t end_task_sect

t: tarea | t tarea 

tarea: pretask id nombre estado descripcion notas est_ini est_end ini end prioridad tipo est_time time posttask

end_task_sect: {
	end_task_section();
}

start_task_sect: {
	init_task_section();
}

pretask: {
	pre_task();
}

posttask: {
	post_task();
}

id: ID FIELD {
	function_id($2);
}

nombre: NAME FIELD {
	function_name($2);
}

estado: STATE FIELD {
	function_state($2);
}

descripcion: DESCRIPTION FIELD {
	function_desc($2);
}

notas: NOTES FIELD {
	function_notes($2);
}

est_ini: EDATE_INI FIELD {
	function_est_date_ini($2);
}

est_end: EDATE_END FIELD {
	function_est_date_end($2);
}

ini: RDATE_INI FIELD {
	function_real_date_ini($2);
}

end: RDATE_END FIELD {
	function_real_date_end($2);
}

prioridad: PRIORITY FIELD {
	function_priority($2);
}

tipo: TYPE FIELD {
	function_type($2);
}

est_time: E_TIME FIELD {
	function_est_time($2);
}

time: R_TIME FIELD {
	function_time($2);
}

nombre: /* vacio */ {
	function_empty_name();
}

prioridad: /* vacio */ {
	function_empty_priority();
}

tipo: /* vacio */ {
	function_empty_type();
}

estado: /* vacio */ {
	function_empty_state();
}

descripcion: /* vacio */ {
	function_empty_desc();
}

notas: /* vacio */ {
	function_empty_notes();
}

est_ini: /* vacio */ {
	function_empty_est_date_ini();
}

est_end: /* vacio */ {
	function_empty_est_date_end();
}

ini: /* vacio */ {
	function_empty_real_date_ini();
}

end: /* vacio */ {
	function_empty_real_date_end();
}

est_time: /* vacio */ {
	function_empty_est_time();
}

time: /* vacio */ {
	function_empty_time();
}

%%

extern FILE *yyin;

int main(int argv,char *argc[]){
	yyin = fopen(argc[1],"r");
	if(yyin){
		yyparse();	
	}else{
		printf("ERROR: Can't open file successfully\n");
		return 1;		
	}
	fclose(yyin);
	return 0;
}
