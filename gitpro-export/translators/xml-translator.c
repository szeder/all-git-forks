#include <stdio.h>
#include "../iface-translate.h"

#define xmlproject_file "xml_project.xml"

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
	write(xmlproject_file,"\t<Start>%s</Start>\n",est_ini);
}

void function_est_date_end(char *est_end){
	write(xmlproject_file,"\t<Finish>%s</Finish>\n",est_end);
}

void function_real_date_ini(char *ini){
	write(xmlproject_file,"\t<ManualStart>%s</ManualStart>\n",ini);
}

void function_real_date_end(char *end){
	write(xmlproject_file,"\t<ManualFinish>%s</ManualFinish>\n",end);
}

void function_priority(char *prior){
	//write("<priority>%s</priority>\n",prior);
}

void function_type(char *type){
	//write("<type>%s</type>\n",type);
}

void function_est_time(char *est_time){
	write(xmlproject_file,"\t<Duration>%s</Duration>\n",est_time);
}

void function_time(char *time){
	write(xmlproject_file,"\t<ManualDuration>%s</ManualDuration>\n",time);
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
