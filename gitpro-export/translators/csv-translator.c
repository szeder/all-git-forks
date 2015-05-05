#include <stdio.h>
#include "../iface-translate.h"

#define csv_file "csv-task.csv"

/*	Private functions 	*/
void write(char *txt,char *var){
	FILE *f = fopen(csv_file ,"a+");
	if(f!=NULL){
		if(var==NULL){
			fprintf(f,txt);
		}else{
			fprintf(f,txt,var);
		}
		fclose(f);
	}	
}

void init_task_section(){
	
}

void end_task_section(){
}

void pre_task(){
}

void post_task(){
}

void function_id(char *id){
	write("%s,",id);
}

void function_name(char *name){
	write("%s,",name);
}

void function_state(char *state){
	write("%s,",state);
}

void function_desc(char *desc){
	write("%s,",desc);
}

void function_notes(char *notes){
	write("%s,",notes);
}

void function_est_date_ini(char *est_ini){
	write("%s,",est_ini);
}

void function_est_date_end(char *est_end){
	write("%s,",est_end);
}

void function_real_date_ini(char *ini){
	write("%s,",ini);
}

void function_real_date_end(char *end){
	write("%s,",end);
}

void function_priority(char *prior){
	write("%s,",prior);
}

void function_type(char *type){
	write("%s,",type);
}

void function_est_time(char *est_time){
	write("%s,",est_time);
}

void function_time(char *time){
	write("%s\n",time);
}

void function_empty_name(){
	write(",",NULL);
}

void function_empty_priority(){
	write(",",NULL);
}

void function_empty_id(){
	write(",",NULL);
}

void function_empty_state(){
	write(",",NULL);
}

void function_empty_desc(){
	write(",",NULL);
}

void function_empty_notes(){
	write(",",NULL);
}

void function_empty_est_date_ini(){
	write(",",NULL);
}

void function_empty_est_date_end(){
	write(",",NULL);
}

void function_empty_real_date_ini(){
	write(",",NULL);
}

void function_empty_real_date_end(){
	write(",",NULL);
}

void function_empty_type(){
	write(",",NULL);
}

void function_empty_est_time(){
	write(",",NULL);
}

void function_empty_time(){
	write(",",NULL);
}
