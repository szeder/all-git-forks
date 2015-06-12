
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "standarization-API.h"

#define std_file "standard-file"

int check = 0;

char *tid;
char *tname;
char *tstate;
char *tdesc;
char *tnotes;
char *test_ini;
char *test_end;
char *tini;
char *tend;
char *tprior;
char *ttype;
char *test_time;
char *ttime;

char *uname;
char *urole;

char *aname;
char *atid;

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

void set_id(char *id){ tid = strdup(id); }
void set_name(char *name){ tname = strdup(name); }
void set_state(char *state){ tstate = strdup(state); }
void set_desc(char *desc){ tdesc = strdup(desc); }
void set_notes(char *notes){ tnotes = strdup(notes); }
void set_est_ini(char *est_ini){ test_ini = strdup(est_ini); }
void set_est_end(char *est_end){ test_end = strdup(est_end); }
void set_ini(char *ini){ tini = strdup(ini); }
void set_end(char *end){ tend = strdup(end); }
void set_priority(char *prior){ tprior = strdup(prior); }
void set_type(char *type){ ttype = strdup(type); }
void set_est_time(char *est_time){ test_time = strdup(est_time); }
void set_time(char *time){ ttime = strdup(time); }

void set_uname(char *username){ uname = strdup(username); }
void set_urole(char *rolename){ urole = strdup(rolename); }

void set_auname(char *name){ aname = strdup(name); }
void set_atid(char *id){ atid = strdup(id); }

void std_task()
{
	if(tid!=NULL){ 
		write("id\n%s\n",tid); free(tid);
	}	
	if(tname!=NULL){ 
		write("name\n%s\n",tname); free(tname);
	}
	if(tstate!=NULL){ 
		write("state\n%s\n",tstate); free(tstate);
	}
	if(tdesc!=NULL){ 
		write("desc\n%s\n",tdesc); free(tdesc);
	}
	if(tnotes!=NULL){ 
		write("notes\n%s\n",tnotes); free(tnotes);
	}
	if(test_ini!=NULL){ 
		write("est-date-ini\n%s\n",test_ini); free(test_ini);
	}
	if(test_end!=NULL){ 
		write("est-date-end\n%s\n",test_end); free(test_end);
	}
	if(tini!=NULL){ 
		write("real-date-ini\n%s\n",tini); free(tini);
	}
	if(tend!=NULL){ 
		write("real-date-end\n%s\n",tend); free(tend);
	}
	if(tprior!=NULL){ 
		write("priority\n%s\n",tprior); free(tprior);
	}
	if(ttype!=NULL){ 
		write("type\n%s\n",ttype); free(ttype);
	}
	if(ttime!=NULL){ 
		write("time\n%s\n",ttime); free(ttime);
	}
	if(test_time!=NULL){ 
		write("est-time\n%s\n",test_time); free(test_time);
	}
	tid = NULL;tname = NULL;tstate = NULL;tdesc = NULL;
	tnotes = NULL;test_ini = NULL;test_end = NULL;
	tini = NULL;tend = NULL;tprior = NULL;ttype = NULL;
	ttime = NULL;test_time = NULL;
}

void std_user()
{
	if(uname!=NULL){
		write("uname\n%s\n",uname); free(uname);
	}
	if(urole!=NULL){
		write("urole\n%s\n",urole); free(urole);
	}
	uname = NULL;urole = NULL;
}

void std_asig()
{
	if(aname!=NULL){
		write("auname\n%s\n",aname); free(aname);
	}
	if(atid!=NULL){
		write("atid\n%s\n",atid); free(atid);
	}
	aname = NULL;atid = NULL;
}
