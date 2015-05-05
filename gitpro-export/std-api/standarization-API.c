
#include <stdio.h>
#include "standarization-API.h"

#define std_file "standard-file"

int check = 0;

/*	Private functions 	*/
void write(char *txt,char *var){
	if(!check){
		remove(std_file);
		check++;
	}
	FILE *f = fopen(std_file,"a+");
	if(f!=NULL){
		fprintf(f,txt,var);
		fclose(f);
	}	
}

/* 	Public 	functions 	*/
void std_id(char *id){
	write("id\n%s\n",id);
}

void std_name(char *name){
	write("name\n%s\n",name);
}

void std_state(char *state){
	write("state\n%s\n",state);
}

void std_desc(char *desc){
	write("desc\n%s\n",desc);
}

void std_notes(char *notes){
	write("notes\n%s\n",notes);
}

void std_est_ini(char *est_ini){
	write("est-date-ini\n%s\n",est_ini);
}

void std_est_end(char *est_end){
	write("est-date-end\n%s\n",est_end);
}

void std_ini(char *ini){
	write("real-date-ini\n%s\n",ini);
}

void std_end(char *end){
	write("real-date-end\n%s\n",end);
}

void std_priority(char *prior){
	write("priority\n%s\n",prior);
}

void std_type(char *type){
	write("type\n%s\n",type);
}

void std_est_time(char *est_time){
	write("est-time\n%s\n",est_time);
}

void std_time(char *time){
	write("time\n%s\n",time);
}
