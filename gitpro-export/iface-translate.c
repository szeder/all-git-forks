#include <stdio.h>
#include <string.h>

/* Implementations */

/* Remember to free returned string */
char *get_simple_uid(char *uname){
	int id = 0;
	char *aux;
	for(aux=uname; *aux; aux++){
		id = id + (int) *aux;
	}
	char strId[25];
	sprintf(strId,"%d",id);
	return strdup(strId);
}

void write(char *filename,char *txt,char *var){
	FILE *f = fopen(filename ,"a+");
	if(f!=NULL){
		if(var==NULL){
			fprintf(f,txt);
		}else{
			fprintf(f,txt,var);
		}
		fclose(f);
	}	
}
