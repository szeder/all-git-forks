#include <stdio.h>
#include <string.h>

/* Syntax sugar to manage add and remove options of link command */
#define LINK_ADD 0
#define LINK_REMOVE 1

/* Syntax sugar to manage add and remove options of assign command */
#define ASSIGN_ADD 2
#define ASSIGN_REMOVE 3

/* Common help functions */
int strstrlen(const char **a){
	int count=0;
	while(*a!=NULL){
		count++;
		a++;
	}
	return count;
} 

/* Creates a new task */
void create_task(const char *name,const char *description,const char *type,const char *priority,
				 const char *estimateTime,const char *initDate,const char *endDate);

/* Link or unlink a list of files and folders to a given task */ 
void link_task(int linkOption, const char *taskId,const char **filesAndFolders);

/* Assign or unassign a list of users to a given task */
void assign_task(int assignOption, const char *taskId,const char **users);

/* Remove an existent task */
void remove_task(const char *taskId);

/* Update different values of a task */
void update_task(const char *taskId,const char *state,const char *priority,const char *estime,
				 const char *ini,const char *end,const char *note);

/* Makes a filtered search of tasks on depending given parameters */
void filter_task(const char *taskId, const char *username, const char *state, const char *type,
				 const char *priority, const char *ini, const char *end, const char *text);

/* Main code of task command */
int cmd_task(int argc, const char **argv, const char *prefix){
	if(argc>1){
		/* Create option */
		if(strcmp(argv[1],"-c")==0){
			if(argc==9){
				create_task(argv[2],argv[3],argv[4],argv[5],argv[6],argv[7],argv[8]);
			}else{
				printf("Format: -c name description type priority estimated_time init_date end_date\n");
			}
		/* Link option */
		}else if(strcmp(argv[1],"link")==0){
			if(argc>=5){
				const char **filesAndFolders = &argv[4];		
				if(strcmp(argv[2],"-add")==0){
					link_task(LINK_ADD,argv[3],filesAndFolders);
				}else if(strcmp(argv[2],"-rm")==0){
					link_task(LINK_REMOVE,argv[3],filesAndFolders);
				}else{
					printf("Format: link [-add | -rm] task_id [files_or_folders]\n");
				}
			}else{
				printf("Format: link [-add | -rm] task_id [file_or_folder_1 ... file_or_folder_n]\n");
			}
		/* Assign option */
		}else if(strcmp(argv[1],"assign")==0){
			if(argc>=5){
				const char **users = &argv[4];		
				if(strcmp(argv[2],"-add")==0){
					assign_task(ASSIGN_ADD,argv[3],users);
				}else if(strcmp(argv[2],"-rm")==0){
					assign_task(ASSIGN_REMOVE,argv[3],users);
				}else{
					printf("Format: assign [-add | -rm] task_id [user_1 ... user_n]\n");
				}
			}else{
				printf("Format: assign [-add | -rm] task_id [user_1 ... user_n]\n");
			}
		/* Remove option */
		}else if(strcmp(argv[1],"-r")==0){
			if(argc==3){
				remove_task(argv[2]);
			}else{
				printf("Format: -r task_id\n");
			}
		/* Update option */
		}else if(strcmp(argv[1],"-u")==0){
			if(argc>=5){
				const char **updateParams = &argv[3];
				int paramNumber = strstrlen(updateParams)/2;
				const char *state = NULL; 
				const char *priority = NULL; 
				const char *estime = NULL; 
				const char *ini = NULL; 
				const char *end = NULL;
				const char *note = NULL;
				int i=0;
				int pos=0;
				for(i=0;i<paramNumber;i++){
					if(strcmp(updateParams[pos],"-state")==0){
						state = strdup(updateParams[pos+1]);
					}else if(strcmp(updateParams[pos],"-priority")==0){					
						priority = strdup(updateParams[pos+1]);
					}else if(strcmp(updateParams[pos],"-estime")==0){
						estime = strdup(updateParams[pos+1]);
					}else if(strcmp(updateParams[pos],"-ini")==0){
						ini = strdup(updateParams[pos+1]);
					}else if(strcmp(updateParams[pos],"-end")==0){
						end = strdup(updateParams[pos+1]);
					}else if(strcmp(updateParams[pos],"-addNote")==0){
						note = strdup(updateParams[pos+1]);
					}
					pos=pos+2;
				}
				update_task(argv[2],state,priority,estime,ini,end,note);
			}else{
				printf("Format: -u task_id [-state s | -priority p | -estime t | -ini i | -end e | -addNote n]\n");
			}
		/* Search option */
		}else{
			if(argc>=3){
				const char **filterParams = &argv[1];
				int paramNumber = strstrlen(filterParams)/2;
				printf("paramNumber %d\n",paramNumber);
				const char *taskId = NULL;
				const char *username = NULL;
				const char *state = NULL; 
				const char *type = NULL;
				const char *priority = NULL; 
				const char *ini = NULL; 
				const char *end = NULL;
				const char *contained_text = NULL;
				int i=0;
				int pos=0;
				for(i=0;i<paramNumber;i++){
					if(strcmp(filterParams[pos],"--id")==0){
						taskId = strdup(filterParams[pos+1]);
					}else if(strcmp(filterParams[pos],"--user")==0){					
						username = strdup(filterParams[pos+1]);
					}else if(strcmp(filterParams[pos],"--state")==0){
						state = strdup(filterParams[pos+1]);
					}else if(strcmp(filterParams[pos],"--type")==0){
						type = strdup(filterParams[pos+1]);
					}else if(strcmp(filterParams[pos],"--priority")==0){
						priority = strdup(filterParams[pos+1]);
					}else if(strcmp(filterParams[pos],"--ini")==0){
						ini = strdup(filterParams[pos+1]);
					}else if(strcmp(filterParams[pos],"--end")==0){
						end = strdup(filterParams[pos+1]);
					}else if(strcmp(filterParams[pos],"--contain_text")==0){
						contained_text = strdup(filterParams[pos+1]);
					}
					pos=pos+2;
				}
				filter_task(taskId,username,state,type,priority,ini,end,contained_text);
			}else{
				printf("Format: --id task_id | --user u | --state s | --type t | --priority p | -- ini i | --end e | --contain_text ct\n");
			}
		}
	}else{
		printf("Incorrect command format\n");
		return 0;
	}
	return 1;
}

void create_task(const char *name,const char *description,const char *type,const char *priority,
				 const char *estimateTime,const char *initDate,const char *endDate){
	printf("Creating new task...\n\n");
	printf("Name           : %s\n",name);
	printf("Description    : %s\n",description);
	printf("Type           : %s\n",type);
	printf("Priority       : %s\n",priority);
	printf("Estimated Time : %s\n",estimateTime);
	printf("Init Date      : %s\n",initDate);
	printf("End Date       : %s\n",endDate);
	//Initially state is new
	printf("State          : NEW\n");
}

void link_task(int linkOption, const char *taskId,const char **filesAndFolders){
	printf("Managing asociated task files...\n\n");
	printf("Task Id : %s\n",taskId);
	if(linkOption==LINK_ADD){
		printf("Adding files and folders to task...\n");		
	}else if(linkOption==LINK_REMOVE){
		printf("Removing files and folders from task...\n");
	}
	int i = 0;	
	int len = strstrlen(filesAndFolders);
	while(i < len){
		printf("File %d : %s\n",i,filesAndFolders[i]);
		i++;	
	}
}

void assign_task(int assignOption, const char *taskId,const char **users){
	printf("Managing assigned task users...\n\n");
	printf("Task Id : %s\n",taskId);
	if(assignOption==ASSIGN_ADD){
		printf("Adding users to task...\n");		
	}else if(assignOption==ASSIGN_REMOVE){
		printf("Removing users from task...\n");
	}
	int i = 0;	
	int len = strstrlen(users);
	while(i < len){
		printf("User %d : %s\n",i,users[i]);
		i++;	
	}
}

void remove_task(const char *taskId){
	printf("Removing task...\n\n");
	printf("Task Id : %s\n",taskId);
}

void update_task(const char *taskId,const char *state,const char *priority,const char *estime,
				 const char *ini,const char *end,const char *note){
	printf("Updating task...\n\n");
	printf("Task Id        : %s\n",taskId);	
	if(state!=NULL){
		printf("State          : %s\n",state);
	}
	if(priority!=NULL){
		printf("Priority       : %s\n",priority);
	}
	if(estime!=NULL){
		printf("Estimated Time : %s\n",estime);
	}
	if(ini!=NULL){
		printf("Init Date      : %s\n",ini);
	}
	if(end!=NULL){
		printf("End Date       : %s\n",end);
	}
	if(note!=NULL){
		printf("Note           : %s\n",note);
	}
}

void filter_task(const char *taskId, const char *username, const char *state, const char *type,
				 const char *priority, const char *ini, const char *end, const char *text){
	printf("Searching and filtering tasks...\n\n");
	if(taskId!=NULL){
		printf("Task Id        : %s\n",taskId);	
	}	
	if(username!=NULL){
		printf("Username       : %s\n",username);	
	}
	if(state!=NULL){
		printf("State          : %s\n",state);
	}
	if(type!=NULL){
		printf("Type           : %s\n",type);
	}
	if(priority!=NULL){
		printf("Priority       : %s\n",priority);
	}
	if(ini!=NULL){
		printf("Init Date      : %s\n",ini);
	}
	if(end!=NULL){
		printf("End Date       : %s\n",end);
	}
	if(text!=NULL){
		printf("Contained Text : %s\n",text);
	}
}
