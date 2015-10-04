#include <stdio.h>
#include <string.h>
#include "../iface-translate.h"

#define ganttproject_file "ganttproject.gan"

int ini_real = 0;
int fin_real = 0;
int ini_est = 0;
char *aux_ini_est = NULL;
char *aux_ini_real = NULL;
char *aux_est_time = NULL;
char *aux_prior = NULL;
char *aux_type = NULL;
char *aux_state = NULL;

/* Private functions */

/* Convert hours to days and return char pointer */
char *hours_to_days(char *hours){
	char *aux = (char *) malloc(10);
	sprintf(aux,"%d",(atoi(hours)/8));
	return aux;
}

/* Transform date format from dd/mm/yyyy to yyyy-mm-dd */
char *transform_date(char *date){
	char *aux = (char *) malloc(strlen(date)+1);
	char *d = strtok(date,"/");
	char *m = strtok(NULL,"/");
	char *y = strtok(NULL,"/");
	sprintf(aux,"%s-%s-%s",y,m,d);
	return aux;
}


/* Public functions */
void init_doc(){
	write(ganttproject_file,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",NULL);
	write(ganttproject_file,"<project name=\"Exported GitPro Project\" >\n",NULL);
}

void end_doc(){
	write(ganttproject_file,"\t<taskdisplaycolumns>\n",NULL);
	write(ganttproject_file,"\t\t<displaycolumn property-id=\"state\" order=\"3\" width=\"20\" visible=\"true\" />\n",NULL);
	write(ganttproject_file,"\t\t<displaycolumn property-id=\"prior\" order=\"4\" width=\"20\" visible=\"true\" />\n",NULL);
	write(ganttproject_file,"\t\t<displaycolumn property-id=\"type\" order=\"5\" width=\"20\" visible=\"true\" />\n",NULL);
	write(ganttproject_file,"\t</taskdisplaycolumns>\n",NULL);
	write(ganttproject_file,"</project>\n",NULL);
}

void init_task_section(){
	write(ganttproject_file,"\t<tasks>\n",NULL);
	write(ganttproject_file,"\t\t<taskproperties>\n",NULL);
	write(ganttproject_file,"\t\t\t<taskproperty id=\"prior\" name=\"Priority\" type=\"custom\" valuetype=\"text\" />\n",NULL);
	write(ganttproject_file,"\t\t\t<taskproperty id=\"type\" name=\"Type\" type=\"custom\" valuetype=\"text\" />\n",NULL);
	write(ganttproject_file,"\t\t\t<taskproperty id=\"state\" name=\"State\" type=\"custom\" valuetype=\"text\" />\n",NULL);
	write(ganttproject_file,"\t\t</taskproperties>\n",NULL);
}

void end_task_section(){
	write(ganttproject_file,"\t</tasks>\n",NULL);
}

void pre_task(){
	write(ganttproject_file,"\t\t<task",NULL);
}

void post_task(){
	write(ganttproject_file," >\n",NULL);
	if(aux_prior!=NULL){
		write(ganttproject_file,"\t\t\t<customproperty taskproperty-id=\"prior\" value=\"%s\" />\n",aux_prior);
		free(aux_prior);
		aux_prior=NULL;
	}
	if(aux_type!=NULL){
		write(ganttproject_file,"\t\t\t<customproperty taskproperty-id=\"type\" value=\"%s\" />\n",aux_type);
		free(aux_type);
		aux_type=NULL;
	}
	if(aux_state!=NULL){
		write(ganttproject_file,"\t\t\t<customproperty taskproperty-id=\"state\" value=\"%s\" />\n",aux_state);
		free(aux_state);
		aux_state=NULL;
	}
	write(ganttproject_file,"\t\t</task>\n",NULL);
	if(aux_ini_est!=NULL){ 
		free(aux_ini_est);
		aux_ini_est=NULL;
	}
	if(aux_ini_real!=NULL){ 
		free(aux_ini_real);
		aux_ini_real=NULL;
	}
	if(aux_est_time!=NULL){
		free(aux_est_time);
		aux_est_time=NULL;
	}
	ini_est = 0;
	ini_real = 0;
	fin_real = 0;
}

void function_id(char *id){
	write(ganttproject_file," id=\"%s\"",id);
}

void function_name(char *name){
	write(ganttproject_file," name=\"%s\"",name);
}

void function_state(char *state){
	aux_state = strdup(state);
}

void function_desc(char *desc){ }

void function_notes(char *notes){ }

void function_est_date_ini(char *est_ini){
	ini_est = 1;
	aux_ini_est = transform_date(est_ini);
}

void function_est_date_end(char *est_end){
	//char *new = translateDate(est_end);
	//write(ganttproject_file,"\t<Finish>%s</Finish>\n",new);
	//free(new);
}

void function_real_date_ini(char *ini){
	ini_real = 1;
	aux_ini_real = transform_date(ini);
}

void function_real_date_end(char *end){
	fin_real = 1;
}

void function_priority(char *prior){
	aux_prior = strdup(prior);
}

void function_type(char *type){
	aux_type = strdup(type);
}

void function_est_time(char *est_time){
	if(!ini_real && aux_ini_est!=NULL){
		write(ganttproject_file," start=\"%s\"",aux_ini_est);
		//Convert hours to days
		char *days  = hours_to_days(est_time);
		write(ganttproject_file," duration=\"%s\"",days);
		free(days);
	}else{
		aux_est_time = strdup(est_time);
	}
}

void function_time(char *time){
	if(ini_real && aux_ini_real!=NULL){
		write(ganttproject_file," start=\"%s\"",aux_ini_real);
		if(fin_real){
			char *days = hours_to_days(time);
			write(ganttproject_file," duration=\"%s\"",days);
			write(ganttproject_file," complete=\"100\"",NULL);
			free(days);
		}else{
			if(aux_est_time!=NULL){
				char *days = hours_to_days(aux_est_time);
				write(ganttproject_file," duration=\"%s\"",days);
				free(days);
			}
		}
	}	
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

void init_user_section(){
	write(ganttproject_file,"\t<resources>\n",NULL);
}

void end_user_section(){
	write(ganttproject_file,"\t</resources>\n",NULL);
}

void pre_user(){
	write(ganttproject_file,"\t\t<resource",NULL);
}

void post_user(){
	write(ganttproject_file," />\n",NULL);
}

void function_uname(char *uname){
	char *uid = get_simple_uid(uname);
	write(ganttproject_file," id=\"%s\"",uid);
	write(ganttproject_file," name=\"%s\"",uname);
	free(uid);
}

void function_urole(char *urole){ /* No es necesario */ }

void function_empty_urole(){ }

void init_asig_section(){
	write(ganttproject_file,"\t<allocations>\n",NULL);
}

void end_asig_section(){
	write(ganttproject_file,"\t</allocations>\n",NULL);
}

void pre_assignment(){
	write(ganttproject_file,"\t\t<allocation",NULL);
}

void post_assignment(){
	write(ganttproject_file," />\n",NULL);
}

void function_atid(char *atid){
	atid[strlen(atid)-1] = '\0';
	write(ganttproject_file," task-id=\"%s\"",atid);
}

void function_auname(char *auname){
	char *uid = get_simple_uid(auname);
	write(ganttproject_file," resource-id=\"%s\"",uid);
	free(uid);
}


