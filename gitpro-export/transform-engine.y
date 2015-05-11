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

%token <char *> UNAME
%token <char *> UROLE

%token <char *> ATID
%token <char *> AUNAME

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

%type <char *> uname
%type <char *> urole

%type <char *> atid
%type <char *> auname

%type <char *> init_doc
%type <char *> end_doc

%type <char *> t
%type <char *> u
%type <char *> a

%type <char *> ts
%type <char *> us
%type <char *> as

%type <char *> init_task_sect
%type <char *> end_task_sect
%type <char *> pretask
%type <char *> posttask

%type <char *> init_user_sect
%type <char *> end_user_sect
%type <char *> preuser
%type <char *> postuser

%type <char *> init_asig_sect
%type <char *> end_asig_sect
%type <char *> preasig
%type <char *> postasig

%type <char *> task
%type <char *> user
%type <char *> asig


/* Definimos el simbolo inicial */
%start s

%%

s: init_doc ts us as end_doc

init_doc: {
	init_doc();
}

end_doc: {
	end_doc();
}

ts: /* vacio */ | init_task_sect t end_task_sect

us: /* vacio */ | init_user_sect u end_user_sect

as: /* vacio */ | init_asig_sect a end_asig_sect

t: task | t task 

u: user | u user

a: asig | a asig

task: pretask id nombre estado descripcion notas est_ini est_end ini end prioridad tipo est_time time posttask

user: preuser uname urole postuser

asig: preasig atid auname postasig

init_task_sect: {
	init_task_section();
}

end_task_sect: {
	end_task_section();
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

init_user_sect: {
	init_user_section();
}

end_user_sect: {
	end_user_section();
}

preuser: {
	pre_user();
}

postuser: {
	post_user();
}

uname: UNAME FIELD {
	function_uname($2);
}

urole: UROLE FIELD {
	function_urole($2);
}

urole: {
	function_empty_urole();
}

init_asig_sect: {
	init_user_section();
}

end_asig_sect: {
	end_user_section();
}

preasig: {
	pre_assignment();
}

postasig: {
	post_assignment();
}

atid: ATID FIELD {
	function_atid($2);
}

auname: AUNAME FIELD {
	function_auname($2);
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
