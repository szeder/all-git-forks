#include <stdio.h>
#include "../iface-translate.h"

#define xmlproject_file "xml_project.xml"

/*	Private functions 	*/
void write(char *txt,char *var){
	FILE *f = fopen(xmlproject_file ,"a+");
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
	write("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n",NULL);
	write("<Project xmlns=\"http://schemas.microsoft.com/project\">\n",NULL);
}

void end_task_section(){
	write("</Project>\n",NULL);
}

void pre_task(){
	write("<Task>\n",NULL);
}

void post_task(){
	write("</Task>\n",NULL);
}

void function_id(char *id){
	write("\t<ID>%s</ID>\n",id);
}

void function_name(char *name){
	write("\t<Name>%s</Name>\n",name);
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
	write("\t<Start>%s</Start>\n",est_ini);
}

void function_est_date_end(char *est_end){
	write("\t<Finish>%s</Finish>\n",est_end);
}

void function_real_date_ini(char *ini){
	write("\t<ManualStart>%s</ManualStart>\n",ini);
}

void function_real_date_end(char *end){
	write("\t<ManualFinish>%s</ManualFinish>\n",end);
}

void function_priority(char *prior){
	//write("<priority>%s</priority>\n",prior);
}

void function_type(char *type){
	//write("<type>%s</type>\n",type);
}

void function_est_time(char *est_time){
	write("\t<Duration>%s</Duration>\n",est_time);
}

void function_time(char *time){
	write("\t<ManualDuration>%s</ManualDuration>\n",time);
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
