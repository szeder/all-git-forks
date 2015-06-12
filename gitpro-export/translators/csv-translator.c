#include <stdio.h>
#include "../iface-translate.h"

#define csv_file "csv-task.csv"

void init_doc(){ }
void end_doc(){ }

void init_task_section(){ }
void end_task_section(){ 
	write(csv_file,"break\n",NULL);
}

void pre_task(){ }
void post_task(){ }

void init_user_section(){ }
void end_user_section(){
	write(csv_file,"break\n",NULL); 
}

void pre_user(){ }
void post_user(){ }

void function_urole(char *urole){
	write(csv_file,"%s\n",urole);
}

void function_empty_urole(){ 
	write(csv_file,"%s\n","PUBLIC");
}

void function_uname(char *uname){
	write(csv_file,"%s,",uname);
}

void init_asig_section(){ }
void end_asig_section(){ }

void pre_assignment(){ }
void post_assignment(){ }

void function_atid(char *atid){
	write(csv_file,"%s\n",atid);
}

void function_auname(char *auname){
	write(csv_file,"%s,",auname);
}

void function_id(char *id){
	write(csv_file,"%s,",id);
}

void function_name(char *name){
	write(csv_file,"%s,",name);
}

void function_state(char *state){
	write(csv_file,"%s,",state);
}

void function_desc(char *desc){
	write(csv_file,"%s,",desc);
}

void function_notes(char *notes){
	write(csv_file,"%s,",notes);
}

void function_est_date_ini(char *est_ini){
	write(csv_file,"%s,",est_ini);
}

void function_est_date_end(char *est_end){
	write(csv_file,"%s,",est_end);
}

void function_real_date_ini(char *ini){
	write(csv_file,"%s,",ini);
}

void function_real_date_end(char *end){
	write(csv_file,"%s,",end);
}

void function_priority(char *prior){
	write(csv_file,"%s,",prior);
}

void function_type(char *type){
	write(csv_file,"%s,",type);
}

void function_est_time(char *est_time){
	write(csv_file,"%s,",est_time);
}

void function_time(char *time){
	write(csv_file,"%s\n",time);
}

void function_empty_name(){
	write(csv_file,",",NULL);
}

void function_empty_priority(){
	write(csv_file,",",NULL);
}

void function_empty_id(){
	write(csv_file,",",NULL);
}

void function_empty_state(){
	write(csv_file,",",NULL);
}

void function_empty_desc(){
	write(csv_file,",",NULL);
}

void function_empty_notes(){
	write(csv_file,",",NULL);
}

void function_empty_est_date_ini(){
	write(csv_file,",",NULL);
}

void function_empty_est_date_end(){
	write(csv_file,",",NULL);
}

void function_empty_real_date_ini(){
	write(csv_file,",",NULL);
}

void function_empty_real_date_end(){
	write(csv_file,",",NULL);
}

void function_empty_type(){
	write(csv_file,",",NULL);
}

void function_empty_est_time(){
	write(csv_file,",",NULL);
}

void function_empty_time(){
	write(csv_file,",\n",NULL);
}
