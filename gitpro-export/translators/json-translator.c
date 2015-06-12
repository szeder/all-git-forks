#include <stdio.h>
#include "../iface-translate.h"

#define json_file "json-task.json"

void init_doc(){ 
	write(json_file,"{\n",NULL); 
}
void end_doc(){ 
	write(json_file,"}\n",NULL);
}

void init_task_section(){ 
	write(json_file,"\t\"task\": [\n",NULL); 
}

void end_task_section(){ 
	write(json_file,"\t]\n",NULL); 
}

void pre_task(){ 
	write(json_file,"\t\t{",NULL);
}
void post_task(){ 
	write(json_file,"}",NULL);
}

void init_user_section(){ 
	write(json_file,"\t\"user\": [\n",NULL);
}

void end_user_section(){
	write(json_file,"\t]\n",NULL); 
}

void pre_user(){ 
	write(json_file,"\t\t{",NULL);	
}
void post_user(){ 
	write(json_file,"}",NULL);	
}

void function_urole(char *urole){
	write(json_file,"\"role\":\"%s\" ",urole);
}

void function_empty_urole(){ 
	write(json_file,"\"role\":\"%s\" ","PUBLIC");
}

void function_uname(char *uname){
	write(json_file,"\"username\":\"%s\", ",uname);
}

void init_asig_section(){
	write(json_file,"\t\"asig\": [\n",NULL); 
}

void end_asig_section(){ 
	write(json_file,"\t]\n",NULL);
}

void pre_assignment(){ 
	write(json_file,"\t\t{",NULL);
}
void post_assignment(){ 
	write(json_file,"}",NULL);
}

void function_atid(char *atid){
	write(json_file,"\"task-id\":\"%s\" ",atid);
}

void function_auname(char *auname){
	write(json_file,"\"username\":\"%s\",",auname);
}

void function_id(char *id){
	write(json_file,"\"task-id\":\"%s\", ",id);
}

void function_name(char *name){
	write(json_file,"\"name\":\"%s\", ",name);
}

void function_state(char *state){
	write(json_file,"\"state\":\"%s\", ",state);
}

void function_desc(char *desc){
	write(json_file,"\"description\":\"%s\", ",desc);
}

void function_notes(char *notes){
	write(json_file,"\"notes\":\"%s\", ",notes);
}

void function_est_date_ini(char *est_ini){
	write(json_file,"\"estimated-start-date\":\"%s\", ",est_ini);
}

void function_est_date_end(char *est_end){
	write(json_file,"\"estimated-end-date\":\"%s\", ",est_end);
}

void function_real_date_ini(char *ini){
	write(json_file,"\"real-start-date\":\"%s\", ",ini);
}

void function_real_date_end(char *end){
	write(json_file,"\"real-end-date\":\"%s\", ",end);
}

void function_priority(char *prior){
	write(json_file,"\"priority\":\"%s\", ",prior);
}

void function_type(char *type){
	write(json_file,"\"type\":\"%s\", ",type);
}

void function_est_time(char *est_time){
	write(json_file,"\"estimated-duration-hours\":\"%s\", ",est_time);
}

void function_time(char *time){
	write(json_file,"\"real-duration-hours\":\"%s\" ",time);
}

void function_empty_name(){ }
void function_empty_priority(){ }
void function_empty_id(){ }
void function_empty_state(){ }
void function_empty_desc(){ }
void function_empty_notes(){ }
void function_empty_est_date_ini(){ }
void function_empty_est_date_end(){ }
void function_empty_real_date_ini(){ }
void function_empty_real_date_end(){ }
void function_empty_type(){ }
void function_empty_est_time(){ }
void function_empty_time(){ }
