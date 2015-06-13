%{
#include "std-api/standarization-API.h"

/* Temporal data of users (name and id) to resolve id into name 
	in execution time */
struct userdata {
	char *name; //Unique user name
	int id; //Role asigned to user
	struct userdata *next; //pointer to next user node
};	

typedef struct userdata *templist;

/* Add a new user to temporal list */
void insert_user(templist *list,char *name,int id){
	if((*list)==NULL){
	 	(*list) = (templist) malloc(sizeof(struct userdata));
		(**list).name = strdup(name);
		(**list).id = id;
		(**list).next = NULL;
	}else{
		insert_user(&((*list)->next),name,id);
	}
}

/* Dealloc temporal list from memory */
void dealloc_users(templist list){
	templist aux;	
	while(list!=NULL){
		aux = list;
		list=(*list).next;
		free((*aux).name);
		free(aux);
	}
}

/* Returns name of user given its user id */
char *get_username(templist list, int id){
	char *username = NULL;
	while(list!=NULL && username==NULL){
		if(list->id == id){
			username = strdup(list->name);
		}		
		list = list->next;
	}
	return username;
}

#define DATE 99
#define DURATION 100

/* List where temporal user data will be allocated */
templist mylist;

/* Temporal data to resolve start and end date. And duration too */
char *date = NULL;
char *duration = NULL;

/* Temporal data from task */
char *type;
char *prior;
char *state;

/* Temporal data to collect from users */
int userid;
char *username;

int data = NONE;
int tvalue = -1;
int uvalue = -1;
int avalue = -1;

int inCustomProperty = 0;

int inProgress = 0;
int complete = 0;

/* Private functions */

/* Convert days to hours and return char pointer */
char *days_to_hours(char *days){
	char *aux = (char *) malloc(10);
	sprintf(aux,"%d",(atoi(days)*8));
	return aux;
}

/* Transform date format from yyyy-mm-dd to dd/mm/yyyy */
char *transform_date(char *mydate){
	char *aux = (char *) malloc(strlen(mydate)+1);
	char *y = strtok(mydate,"-");
	char *m = strtok(NULL,"-");
	char *d = strtok(NULL,"-");
	sprintf(aux,"%s/%s/%s",d,m,y);
	return aux;
}

/* Calculate end date from start date and duration in days */
char *calculate_end_date(char *mydate, char *myduration){
	char *end = (char *) malloc(strlen(mydate)+1);
	char *year = strtok(date,"-");
	char *month = strtok(NULL,"-");
	char *day = strtok(NULL,"-");
		
	int y = atoi(year);
	int m = atoi(month);
	int d = atoi(day);
	int days = atoi(myduration);
	
	//Substract 1 cause it ends at laboral day. For example, if 
	//1 laboral day starts and ends at same day
	d = d+days-1;
	
	//Month with 31 days and standard behavior
	if(m==4 || m==6 || m==6 || m==10 || m==12){
		if(d>31){
			m = m+1;
			d = d-31;
		}
	}
	//February (28 days or 29 if leap year
	else if(m==2){
		if(y%4==0 && ( !((y%100)==0) || (y%400)==0)){
			if(d>29){
				m=m+1;
				d=d-29;
			}
		}else{
			if(d>28){
				m=m+1;
				d=d-28;
			}
		}
	}
	//Month with 30 days
	else{
		if(d>30){
			m = m+1;
			d = d-30;
		}
	}
	
	if(m>12){
		y=y+1;
		m=m-12;
	}
	
	if(d<10){	
		sprintf(end,"0%d/%d/%d",d,m,y);
	}else if(m<10){
		sprintf(end,"%d/0%d/%d",d,m,y);	
	}else if(d<10 && m<10){
		sprintf(end,"0%d/0%d/%d",d,m,y);
	}else{
		sprintf(end,"%d/%d/%d",d,m,y);
	}
	return end;

}

void process_task_datetime(){
	if(data==TASK && date!=NULL && duration!=NULL){
		char *auxdate = strdup(date);
		char *start = transform_date(auxdate);
		char *end = calculate_end_date(date,duration);
		char *time = days_to_hours(duration);									
		//If task is complete then start and end date are real.
		//Duration too
		if(complete){
			set_ini(start);
			set_time(time);
			set_end(end);
		}
		//If task isnt complete but in progress, start date is real,
		//end date is estimated and duration is estimated
		else if(!complete && inProgress){
			set_ini(start);
			set_est_end(end);
			set_est_time(time);
		//If task isnt complete and not in progress, start date, end date 
		//and duration is estimated
		}else if(!complete && !inProgress){
			set_est_ini(start);
			set_est_end(end);
			set_est_time(time);
		}
		free(auxdate);
		free(date);
		free(duration);
		date=NULL;duration=NULL;
		free(end);free(start);free(time);
	}
}

%}

%option noyywrap

intro [\n]

cadena [^=<>\"\n]

%%

(?i:"<task ")				{ data=TASK; inProgress =0; complete=0; }
(?i:"<customproperty ")		{ inCustomProperty = 1; }
(?I:"</customproperty>")	{ inCustomProperty = 0; }
(?i:"/>")					{ 
							  switch(data){
								case TASK: process_task_datetime();
										   if(!inCustomProperty){ 
												std_task(); 
												inProgress=0;
												complete=0; } 
										   break;
								case USER: std_user();break;
								case ASIG: std_asig();break;
							  }
							  inCustomProperty = 0;
							}
(?i:"</task>")				{ process_task_datetime(); 
							  inProgress=0;
							  complete=0;
							  std_task(); }

(?i:"<resources>")			{ data=USER; }
(?i:"<resource ")			{ data=USER; }
(?i:"</resources>")			{ data=NONE; }

(?i:"<allocations>")		{ data=ASIG; }
(?i:"<allocation ")			{ data=ASIG; }
(?i:"</allocations>")		{ data=NONE; }

"\" "						{ tvalue=NONE; uvalue=NONE; avalue=NONE; }

(?i:"id=\"")				{ tvalue = ID;  uvalue=UID; }
(?i:"name=\"")				{ tvalue = NAME; uvalue = UNAME; }

(?i:"\"state\" value=\"")	{ tvalue = STATE; }
(?i:"\"type\" value=\"")	{ tvalue = TYPE; }
(?i:"\"prior\" value=\"")	{ tvalue = PRIOR; }
(?i:"start=\"")				{ tvalue = DATE;}
(?i:"complete=\"")			{ inProgress = 1; }
(?i:"complete=\"0\"")		{ inProgress = 0; }
(?i:"duration=\"")			{ tvalue = DURATION; }
(?i:"complete=\"100\"")		{ complete = 1; }

(?i:"task-id=\"")			{ avalue = ATID; }
(?i:"resource-id=\"")		{ avalue = AUNAME; }

{intro}				{ /* vacio */ }

{cadena}+			{ 	
						switch(data){
							case TASK:
								switch(tvalue){
									case ID: set_id(yytext); break;
									case NAME: set_name(yytext); break;
									case STATE: set_state(yytext); break;
									case DESC: set_desc(yytext); break;
									case NOTES: set_notes(yytext); break;
									case EST_INI: set_est_ini(yytext); break; 
									case EST_END: set_est_end(yytext); break;
									case INI: set_ini(yytext); break;
									case END: set_end(yytext);break;
									case PRIOR: set_priority(yytext); break;
									case TYPE: set_type(yytext); break;
									case EST_TIME: set_est_time(yytext); break;
									case TIME: set_time(yytext); tvalue=ID; break;
									case DATE: date = strdup(yytext); break;
									case DURATION: duration = strdup(yytext); break;
								}
								break;
							case USER:
								switch(uvalue){
									case UID: userid = atoi(yytext); break;
									case UNAME: set_uname(yytext); insert_user(&mylist,yytext,userid); break;
								}
								break;
							case ASIG:
								switch(avalue){
									case ATID: set_atid(yytext); break;
									case AUNAME: username = get_username(mylist,atoi(yytext)); set_auname(username); free(username); avalue=ATID; break;
								}
								break;
						}
					}

.					{ /* vacio */ }

%%

main(){
	yylex();
	dealloc_users(mylist);
}
