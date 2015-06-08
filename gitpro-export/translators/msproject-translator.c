#include <stdio.h>
#include <string.h>
#include "../iface-translate.h"

#define xmlproject_file "msproject.xml"

/* Private functions */
char *translateDate(char *date){
	int size = strlen(date);
	int pos_dest = strlen(date)-1;
	char day[3];
	char month[3];
	char year[5];
	strncpy(day,date,2);
	day[2] = '\0';
	date+=3;
	strncpy(month,date,2);
	month[2] = '\0';
	date+=3;
	strncpy(year,date,4);
	year[4] = '\0';
	char *dest = (char *) malloc(size);
	strcat(strcat(strcat(strcat(strcpy(dest,year),"-"),month),"-"),day);
	return dest;
}

char *translateTime(char *time){
	char hourStr[5];
	char minStr[3];
	char secStr[3];
	int h = atoi(time);
	sprintf(hourStr,"%d",h);
	float aux = atof(time);
	aux -= (float) h;
	aux *= 60.0;
	int m = (int) aux;
	sprintf(minStr,"%d",m);
	aux -= (float) m;
	aux *= 60.0;
	int s = (int) aux;
	sprintf(secStr,"%d",s);
	char *dest = (char *) malloc(2+strlen(hourStr)+1+strlen(minStr)+1+strlen(secStr)+1+1); 
	strcat(strcat(strcat(strcat(strcat(strcat(strcpy(dest,"PT"),hourStr),"H"),minStr),"M"),secStr),"S");
	return dest;
}


/* Public functions */
void init_doc(){
	write(xmlproject_file,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",NULL);
	write(xmlproject_file,"<Project xmlns=\"http://schemas.microsoft.com/project\">\n",NULL);
}

void end_doc(){
	write(xmlproject_file,"</Project>\n",NULL);
}

void init_task_section(){
	write(xmlproject_file,"<Tasks>\n",NULL);
}

void end_task_section(){
	write(xmlproject_file,"</Tasks>\n",NULL);
}

void pre_task(){
	write(xmlproject_file,"<Task>\n",NULL);
}

void post_task(){
	write(xmlproject_file,"</Task>\n",NULL);
}

void function_id(char *id){
	write(xmlproject_file,"\t<UID>%s</UID>\n",id);
	write(xmlproject_file,"\t<ID>%s</ID>\n",id);
	write(xmlproject_file,"\t<Active>1</Active>",NULL);
	write(xmlproject_file,"\t<Manual>1</Manual>",NULL);
	write(xmlproject_file,"\t<IsNull>0</IsNull>",NULL);
}

void function_name(char *name){
	write(xmlproject_file,"\t<Name>%s</Name>\n",name);
}

void function_state(char *state){
	//write("<state>%s</state>\n",state);
}

void function_desc(char *desc){
	//write("<description>%s</description>\n",desc);
}

void function_notes(char *notes){
	//write("<notes>%s</notes>\n",notes);
}

void function_est_date_ini(char *est_ini){
	char *new = translateDate(est_ini);
	write(xmlproject_file,"\t<Start>%s</Start>\n",new);
	free(new);
}

void function_est_date_end(char *est_end){
	char *new = translateDate(est_end);
	write(xmlproject_file,"\t<Finish>%s</Finish>\n",new);
	free(new);
}

void function_real_date_ini(char *ini){
	char *new = translateDate(ini);
	write(xmlproject_file,"\t<ManualStart>%s</ManualStart>\n",new);
	free(new);
}

void function_real_date_end(char *end){
	char *new = translateDate(end);
	write(xmlproject_file,"\t<ManualFinish>%s</ManualFinish>\n",new);
	free(new);
}

void function_priority(char *prior){
	//write("<priority>%s</priority>\n",prior);
}

void function_type(char *type){
	//write("<type>%s</type>\n",type);
}

void function_est_time(char *est_time){
	char *t = translateTime(est_time);
	write(xmlproject_file,"\t<Duration>%s</Duration>\n",t);
	free(t);
}

void function_time(char *time){
	char *t = translateTime(time);
	write(xmlproject_file,"\t<ManualDuration>%s</ManualDuration>\n",t);
	free(t);
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
	write(xmlproject_file,"<Resources>\n",NULL);
}

void end_user_section(){
	write(xmlproject_file,"</Resources>\n",NULL);
}

void pre_user(){
	write(xmlproject_file,"<Resource>\n",NULL);
}

void post_user(){
	write(xmlproject_file,"</Resource>\n",NULL);
}

void function_uname(char *uname){
	char *uid = get_simple_uid(uname);
	write(xmlproject_file,"\t<UID>%s</UID>\n",uid);
	write(xmlproject_file,"\t<ID>%s</ID>\n",uid);
	write(xmlproject_file,"\t<Name>%s</Name>\n",uname);
	free(uid);
}

void function_urole(char *urole){ /* No es necesario */ }

void function_empty_urole(){ }

void init_asig_section(){
	write(xmlproject_file,"<Assignments>\n",NULL);
}

void end_asig_section(){
	write(xmlproject_file,"</Assignments>\n",NULL);
}

void pre_assignment(){
	write(xmlproject_file,"<Assignment>\n",NULL);
}

void post_assignment(){
	write(xmlproject_file,"</Assignment>\n",NULL);
}

void function_atid(char *atid){
	write(xmlproject_file,"\t<TaskUID>%s</TaskUID>\n",atid);
}

void function_auname(char *auname){
	char *uid = get_simple_uid(auname);
	write(xmlproject_file,"\t<ResourceUID>%s</ResourceUID>\n",uid);
	free(uid);
}


