#include <stdlib.h>

void init_doc();
void end_doc();

void init_task_section();
void end_task_section();

void pre_task();
void post_task();

void function_id(char *id);
void function_name(char *name);
void function_state(char *state);
void function_desc(char *desc);
void function_notes(char *notes);
void function_est_date_ini(char *est_ini);
void function_est_date_end(char *est_end);
void function_real_date_ini(char *ini);
void function_real_date_end(char *end);
void function_priority(char *prior);
void function_type(char *type);
void function_est_time(char *est_time);
void function_time(char *time);

/* If a field is empty following functions will execute */
void function_empty_name();
void function_empty_priority();
void function_empty_state();
void function_empty_desc();
void function_empty_notes();
void function_empty_est_date_ini();
void function_empty_est_date_end();
void function_empty_real_date_ini();
void function_empty_real_date_end();
void function_empty_type();
void function_empty_est_time();
void function_empty_time();

void init_user_section();
void end_user_section();

void pre_user();
void post_user();

void function_uname(char *uname);
void function_urole(char *urole);

void function_empty_urole();

void init_asig_section();
void end_asig_section();

void pre_assignment();
void post_assignment();

void function_atid(char *atid);
void function_auname(char *auname);

/* Public implemented functions */

char *get_simple_uid(char *uname);

/*	Private functions 	*/
void write(char *filename,char *txt,char *var);
