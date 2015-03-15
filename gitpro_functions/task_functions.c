#include "task_functions.h"
#include "../gitpro_api/gitpro_data_api.h"
#include "../gitpro_api/db_constants.h"
#include <time.h>
#include "../gitpro_validate/v_codes.h"

/* Declarations to use in private functions */
void rm_assign_task(char *id,char *rm);
void rm_link_files(char *id,char *rm);

#define LOG_FILENAME ".task_logtime"

#define A "NEW"
#define B "IN PROGRESS"
#define C "REJECTED"
#define D "RESOLVED"

// Info to save in temp file for time task logging
struct info {
	int tid;
	time_t init;
};

/* A -> B
 * B -> D
 * A -> C
 * B -> C
 * A -> D */

/*******************/
/*PRIVATE FUNCTIONS*/
/*******************/

/*	Name		: check_state_transition
 *  Parameters	: actual state and transition to
 *  Return		: 1 if ok or 0 in other case
 *  Notes		: Transition states are defined in this function and here is 
 * 		all available states */
int check_state_transition(char *from,char *to){
	if(  (!strcmp(from,A) && !strcmp(to,B))
		|| (!strcmp(from,B) && !strcmp(to,D))
		|| (!strcmp(from,A) && !strcmp(to,C))
		|| (!strcmp(from,B) && !strcmp(to,C))
		|| (!strcmp(from,A) && !strcmp(to,D)) ){
		return 1;
	}else{
		return 0;
	}
}

/*	Name		: check_update_transitions
 * 	Parameters	: Nothing
 * 	Return		: 1 if ok or 0 in other case 
 * 	Note		: Check all task state transitions */
int check_update_transitions(char *state_transition, char *filter ){
	char *query_fields[] = {TASK_STATE,0};
	char *query = NULL;
	query = construct_query(query_fields,from(TASK_TABLE),filter,0,0);
	generic_list data = exec_query(query);
	int result = 1;
	if(data!=NULL){
		task_list tasks = data->task_info;
		if(tasks!=NULL){
			while(tasks!=NULL){
				result = (check_state_transition(tasks->state,state_transition) && result);
				tasks=tasks->next;
			}
		} 
		dealloc(data);
	}
	if(query!=NULL){
		free(query);
	}
	return result;
}

/*	Name		: print_if_pending
 * 	Parameters	: task id
 *  Return		: nothing, prints task data if are in NEW or IN PROGRESS state */
void print_if_pending(int id){
	char *query = NULL;
	char *fields[] = {TASK_STATE,TASK_NAME,0};
	char *f_id = format_number(id);
	char *f_new = format_string(A);
	char *f_inprog = format_string(B);
	char *where_part = where(_and_(cond(TASK_ID,"=",f_id),_or_(cond(TASK_STATE,"=",f_new),cond(TASK_STATE,"=",f_inprog))));
	query = construct_query(fields,from(TASK_TABLE),where_part,NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	if(query!=NULL) free(query);
	if(data!=NULL){
		task_list info = (*data).task_info;
		if(info!=NULL){
			printf("* %d | %s | %s\n",id,info->state,info->task_name);
		}
		dealloc(data);
	}			
	if(f_id!=NULL) free(f_id);if(f_new!=NULL) free(f_new); if(f_inprog!=NULL) free(f_inprog);
	
}


/*	Name 		: print_task_asignations
 * 	Parameters	: task id
 * 	Return		: nothing
 * 	Notes		: prints users asigned to task id given */
void print_task_asignations(id){
	char *query = NULL;
	char *fields[] = {ALL,0};
	char *f_id = format_number(id);
	char *where_part = where(cond(ASIG_TASK_ID,"=",f_id));
	query = construct_query(fields,from(ASIG_TABLE),where_part,NULL,NULL);
	//execute query	
	generic_list data = NULL;
	data = 	exec_query(query);
	free(query);
	//show data
	if((*data).asig_info!=NULL){
		asig_list aux =  (*data).asig_info;
		printf("\tAssigned to: ");
		while(aux!=NULL){
			printf("%s   ",aux->user_name);
			aux=aux->next;
		}
		aux=NULL;
	}else{
		printf("\tNo assigned yet\n");
	}
	dealloc(data);
}

/*	Name 		: print_task_asociations
 * 	Parameters	: task id
 * 	Return		: nothing
 * 	Notes		: prints file asociations to task id given */
void print_task_asociations(id){
	char *query = NULL;
	char *fields[] = {ALL,0};
	char *f_id = format_number(id);
	char *where_part = where(cond(ASOC_TASK_ID,"=",f_id));
	query = construct_query(fields,from(ASOC_TABLE),where_part,NULL,NULL);
	//execute query	
	generic_list data = NULL;
	data = 	exec_query(query);
	free(f_id);
	free(query);
	//show data
	if((*data).asoc_info!=NULL){
		asoc_list aux =  (*data).asoc_info;
		printf("\tAssociated files: \n");
		while(aux!=NULL){
			printf("\n  %s\n",aux->file_path);
			aux=aux->next;
		}
		aux=NULL;
	}else{
		printf("\tNo associated files yet\n");
	}
	dealloc(data);
}

/*	Name		: resolve_ambig
	Parameters	: file name
	Return		: selected file path. if unique it's returned automatically otherwise 
				 is asked to user for a selection
	Used for	: resolve ambig paths in add_link_files and rm_link_files
	Notes		: remember to free returned pointer with free function in stdlib.h */
char *resolve_ambig(char *name){
	char *select = NULL;
	char *query_fields[] = {FILE_PATH,0};
	char *file_name = format_string(name);
	select = construct_query(query_fields,from(FILE_TABLE),where(cond(FILE_NAME,"=",file_name)),0,0);
	generic_list result = exec_query(select);
	char *file_path = NULL;
	if(result!=NULL){
		if(result->file_info!=NULL){
			int i=0;
			file_list aux = result->file_info;
			while(aux!=NULL){
				i++;
				aux = aux->next;
			}
			aux = result->file_info;
			if (i>1) {
				printf("Has found more than one path for file or folder '%s'\n",name);
				printf("Select one [0 - %d] and press ENTER\n",i-1);
				i = 0;
				while(aux!=NULL){
					printf("%d | %s\n",i,aux->file_path);
					i++;
					aux = aux->next;
				}
			}
			if(i==1){
				aux = result->file_info;
				printf("+ Selected file '%s'\n",aux->file_path);
				file_path = format_string(aux->file_path);
			}else{
				char *opt = (char *) malloc(10);
				fgets(opt,10,stdin);
				opt[strlen(opt)-1]='\0';
				if(atoi(opt)<0 || atoi(opt)>i){
					printf("- Can't select path\n");
					dealloc(result);
					return NULL;
				}else{
					int j = 0;
					aux = NULL;
					aux = result->file_info;
					while(j<atoi(opt)){
						aux = aux->next;
						j++;
					}
					file_path = format_string(aux->file_path);
					printf("+ Selected file '%s'\n",aux->file_path);
				}
			}
		}
	}
	free(select);
	free(file_name);
	dealloc(result);
	return file_path;
}

/* 	Name		: rm_asociations_task
	Parameters	: task id
	Return		: nothing
	Used for	: remove task asociations before remove task and maintain consistent data */
void rm_asociations_task(int task_id){
	char *sub_query = NULL;
	char *aux_fields[] = {ASOC_PATH,ASOC_TASK_ID,0};
	char *id = format_number(task_id);
	sub_query = construct_query(aux_fields,from(ASOC_TABLE),where(cond(ASOC_TASK_ID,"=",id)),0,0);
	generic_list temp = NULL;
	temp = exec_query(sub_query);
	if(temp!=NULL){
		if(temp->asoc_info!=NULL){
			asoc_list aux_asoc = temp->asoc_info;
			while(aux_asoc!=NULL){
				char *f[] = {FILE_NAME,0};
				char *f_path = format_string(aux_asoc->file_path);
				char *name_query = construct_query(f,from(FILE_TABLE),where(cond(FILE_PATH,"=",f_path)),0,0);
				generic_list name_list = exec_query(name_query);
				if(name_list!=NULL){
					if(name_list->file_info!=NULL){
						rm_link_files(id,name_list->file_info->file_name);
					}
				}
				free(f_path);
				free(name_query);
				dealloc(name_list);
				aux_asoc=aux_asoc->next;
			}
		}
	}
	dealloc(temp);
	free(sub_query);
	free(id);
}

/* 	Name		: rm_asignations_task
	Parameters	: task id
	Return		: nothing
	Used for	: remove task asignations before remove task and maintain consistent data */
void rm_asignations_task(int task_id){
	char *sub_query = NULL;
	char *id = format_number(task_id);
	char *aux_fields[] = {ASIG_USER_NAME,ASIG_TASK_ID,0};
	sub_query = construct_query(aux_fields,from(ASIG_TABLE),where(cond(ASIG_TASK_ID,"=",id)),0,0);
	generic_list temp = NULL;
	temp = exec_query(sub_query);
	if(temp->asig_info!=NULL){
		asig_list aux_asig = temp->asig_info;
		while(aux_asig!=NULL){
			rm_assign_task(id,aux_asig->user_name);
			aux_asig = aux_asig->next;
		}
	}
	dealloc(temp);
	free(sub_query);
	free(id);
}


/*******************/
/* PUBLIC FUNCTIONS*/
/*******************/

/* See specification in task_functions.h */
void create_task(char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time){
	//Format strings
	char *f_name = format_string(name);
	char *f_state = format_string(state);char *u_state = upper(f_state);
	char *f_desc = NULL; if(desc!=NULL) f_desc=format_string(desc); else{
		f_desc = strdup(NULL_DB); }
	char *f_notes = NULL; if(notes!=NULL) notes=format_string(notes); else{
		f_notes=strdup(NULL_DB);}
	char *f_est_start = NULL; if(est_start!=NULL) f_est_start=format_string(est_start); else{
		f_est_start=strdup(NULL_DB);}
	char *f_est_end = NULL; if(est_end!=NULL) f_est_end=format_string(est_end); else{
		f_est_end=strdup(NULL_DB);}
	char *f_start = NULL; if(start!=NULL) f_start=format_string(start); else{
		f_start=strdup(NULL_DB);}
	char *f_end = NULL; if(end!=NULL) f_end=format_string(end); else{
		f_end=strdup(NULL_DB);}
	char *f_prior = format_string(prior);char *u_prior = upper(f_prior); 
	char *f_type = format_string(type);char *u_type = upper(f_type);
	char *f_est_time = NULL; if(est_time!=NULL) f_est_time = strdup(est_time); else{
		f_est_time=strdup(NULL_DB);}
	char *f_time = NULL; if(time!=NULL) f_time=strdup(time); else{
		f_time=strdup(NULL_DB);}
	//Obtain unique id
	int id = new_task_id();
	char new_id[10+1];
	sprintf(new_id,"%d",id);
	//Construct query
	char *insert = NULL;
	char *field_names[] = {TASK_ID,TASK_NAME,TASK_STATE,TASK_DESCRIP,TASK_NOTES,
				TASK_START_EST,TASK_END_EST,TASK_START_REAL,TASK_END_REAL,TASK_PRIOR,
				TASK_TYPE,TASK_REALTIME,TASK_APROXTIME,0};
	char *field_data[] = {new_id,f_name,u_state,f_desc,f_notes,f_est_start,f_est_end,
				f_start,f_end,u_prior,u_type,f_est_time,f_time,0};
	insert = construct_insert(TASK_TABLE,field_names,field_data);
	//Exec query
	exec_nonquery(insert);
	//Free temporal data
	free(u_state);free(u_type);free(u_prior);
	free(f_name);free(f_state);free(f_desc);free(f_notes);
	free(f_est_start);free(f_est_end);free(f_start);free(f_end);
	free(f_prior);free(f_type);free(f_est_time);free(f_time);
	free(insert);				
}

/* See specification in task_functions.h */
void read_task(char *filter,int verbose){
	char *query = NULL;
	char *fields[] = {ALL,0};
	char *order[] = {TASK_START_EST,0};
	query = construct_query(fields,from(TASK_TABLE),filter,NULL,order_by(order,1));
	//execute query	
	generic_list data = NULL;
	data = 	exec_query(query);
	free(query);
	//show data
	if((*data).task_info!=NULL){
		task_list aux =  (*data).task_info;
		printf("Tasks found\n");
		while(aux!=NULL){
			printf("%d | Name: %s   State: %s   Priority: %s   Type: %s\n",aux->task_id,aux->task_name,aux->state,aux->priority,aux->type);
			if(verbose){
				printf("\tStart\tEstimated: %s\tReal: %s\n",aux->est_start_date,aux->start_date);
				printf("\tEnd  \tEstimated: %s\tReal: %s\n",aux->est_end_date,aux->end_date);
				printf("\tTime \tEstimated: %d\tReal: %d\n",aux->est_time_min,aux->real_time_min);
				print_task_asignations(aux->task_id);
				print_task_asociations(aux->task_id);
				printf("\tDescription: %s\n",aux->description);
				printf("\tNotes: %s\n",aux->notes);
			}
			aux=aux->next;
		}
		aux=NULL;
	}else{
		printf("No task matching\n");
	}
	dealloc(data);
}

/* See specification in task_functions.h*/
void update_task(char *filter,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time){
						
	char *update = NULL;
	char *asig_name = NULL; char *asig_state = NULL; char *asig_desc = NULL; char *asig_notes = NULL;
	char *asig_est_start  = NULL; char *asig_est_end = NULL; char *asig_start = NULL;
	char *asig_end = NULL; char *asig_prior = NULL; char *asig_type = NULL;
	char *asig_est_time = NULL; char *asig_time = NULL;
	
	char *f_name,*f_desc,*f_notes,*f_est_start,*f_est_end,*f_start,*f_end,*f_est_time,*f_time;
	char *f_state,*f_prior,*f_type;
	char *u_state,*u_prior,*u_type;
	
	char *concatDesc, *concatNotes;
	char *add_time;

	char *asignations[13];
	
	int i=0;
	
	if(state!=NULL){
		int check = check_update_transitions(state,filter);
		if (!check){
			printf("Invalid state transition\n");
			exit(0);
		}
	}

	//Prepare query asignations
	if(name!=NULL) {
		f_name = format_string(name);
		asig_name=asig(TASK_NAME,f_name);
		asignations[i]=asig_name;i++;}
	if(state!=NULL){
		f_state = format_string(state);
		u_state = upper(f_state);
		 asig_state=asig(TASK_STATE,u_state);
		 asignations[i]=asig_state;i++;}
	if(desc!=NULL) {
		f_desc = format_string(desc);
		concatDesc = concat(TASK_DESCRIP,f_desc);
		asig_desc=asig(TASK_DESCRIP,concatDesc);
		asignations[i]=asig_desc;i++;}
	if(notes!=NULL){
	 	f_notes = format_string(notes);
	 	concatNotes = concat(TASK_NOTES,f_notes);
	 	asig_notes=asig(TASK_NOTES,concatNotes);
	 	asignations[i]=asig_notes;i++;}
	if(est_start!=NULL){
		f_est_start = format_string(est_start);
		asig_est_start=asig(TASK_START_EST,f_est_start);
		asignations[i]=asig_est_start;i++;}
	if(est_end!=NULL){
		f_est_end = format_string(est_end);
		asig_est_end=asig(TASK_END_EST,f_est_end);
		asignations[i]=asig_est_end;i++;}
	if(start!=NULL){ 
		f_start = format_string(start);
		asig_start=asig(TASK_START_REAL,f_start);
		asignations[i]=asig_start;i++; }
	if(end!=NULL) {
		f_end = format_string(end);
		asig_end=asig(TASK_END_REAL,f_end); 
		asignations[i]=asig_end;i++;}
	if(prior!=NULL) {
		f_prior = format_string(prior);
		u_prior = upper(f_prior);
		asig_prior=asig(TASK_PRIOR,u_prior);
		asignations[i]=asig_prior;i++; }
	if(type!=NULL){
	 	f_type = format_string(type);
	 	u_type = upper(f_type);
	 	asig_type=asig(TASK_TYPE,u_type);
	 	asignations[i]=asig_type;i++; }
	if(est_time!=NULL){
		f_est_time = format_string(est_time); 
		asig_est_time=asig(TASK_APROXTIME,f_est_time);
		asignations[i]=asig_est_time;i++;}
	if(time!=NULL) {
		f_time = format_string(time);
		add_time = (char *) malloc(strlen(TASK_REALTIME)+1+strlen(f_time)+1);
		strcpy(add_time,TASK_REALTIME);
		strcat(add_time,"+");strcat(add_time,f_time);
		asig_time=asig(TASK_REALTIME,add_time); 
		asignations[i]=asig_time;i++;}

	asignations[i]=0;
	
	//Update data
	update = construct_update(TASK_TABLE,asignations,filter);
	exec_nonquery(update);
	
	//Free all temporal data
	free(update);
	if(name!=NULL){free(f_name);}
	if(state!=NULL){free(f_state);free(u_state);}
	if(desc!=NULL){free(f_desc);free(concatDesc);}
	if(notes!=NULL){free(f_notes);free(concatNotes);}
	if(est_start!=NULL){free(f_est_start);}
	if(est_end!=NULL){free(f_est_end);}
	if(start!=NULL){free(f_start);}
	if(end!=NULL){free(f_end);}
	if(prior!=NULL){free(f_prior);free(u_prior);}
	if(type!=NULL){free(f_type);free(u_type);}
	if(est_time!=NULL){free(f_est_time);}
	if(time!=NULL){free(f_time);free(add_time);}
	
	printf("+ Tasks updated successfully\n");
}

/* See specification in task_functions.h */
void add_assign_task(char *id,char *add){
	if(add!=NULL){
		char *aux = strdup(add);
		char *temp = strtok(aux,",");
		char *fields[] = {ASIG_USER_NAME,ASIG_TASK_ID,0};
		char *insert = NULL;
		//Assign task to users
		while(temp!=NULL){ 
			insert=NULL;
			char *user_name = format_string(temp);
			char *values[] = {user_name,id,0};
			insert = construct_insert(ASIG_TABLE,fields,values);
			exec_nonquery(insert);
			free(insert);
			free(user_name);
			printf("+ Asigned user %s\n",temp);
			temp = strtok(NULL,",");
		}
	}
}

/* See specification in task_functions.h */
void rm_assign_task(char *id,char *rm){
	if(rm!=NULL){
		char *aux = strdup(rm);
		char *temp = strtok(aux,",");
		char *delete = NULL;
		//Deassign task from users
		while(temp!=NULL){
			delete=NULL;
			char *user_name = format_string(temp);
			delete = construct_delete(ASIG_TABLE,where(_and_(cond(ASIG_USER_NAME,"=",user_name),cond(ASIG_TASK_ID,"=",id))));
			exec_nonquery(delete);
			free(user_name);
			free(delete);
			printf("- Deasigned user %s\n",temp);
			temp = strtok(NULL,",");
		}
	}
}

/* See specification in task_functions.h */
void add_link_files(char *id,char *add){
	if(add!=NULL){
		char *aux = strdup(add);
		char *temp = strtok(aux,",");
		char *insert = NULL;
		while(temp!=NULL){
			//Resolve path and possible ambig 
			char *file_path = resolve_ambig(temp);
			if(file_path!=NULL){
				//Insert asociation
				insert=NULL;
				char *fields[] = {ASOC_PATH,ASOC_TASK_ID,0};
				char *values[] = {file_path,id,0};
				insert = construct_insert(ASOC_TABLE,fields,values);
				exec_nonquery(insert);
				printf("+ Asociated file %s\n",file_path);
				free(file_path);
				free(insert);
				temp = strtok(NULL,",");
			}else{
				//Finalize 'cause can't select a path
				return;
			}
		}
	}
}

/* See specification in task_functions.h */
void rm_link_files(char *id,char *rm){
	if(rm!=NULL){
		char *aux = strdup(rm);
		char *temp = strtok(aux,",");
		char *delete = NULL;
		while(temp!=NULL){
			//Resolve path and possible ambig
			char *file_path = resolve_ambig(temp);
			if(file_path!=NULL){
				//Deasociate file from task
				delete=NULL;
				char *c1 =cond(ASOC_PATH,"=",file_path);
				char *c2 = cond(ASOC_TASK_ID,"=",id);
				delete = construct_delete(ASOC_TABLE,where(_and_(c1,c2)));
				exec_nonquery(delete);
				free(c2);free(c1);
				printf("- Deasociated file %s\n",file_path);
				//If file has no asociations to other tasks, delete it until user add again
				char * check_query = NULL;
				char *f_count = count(ASOC_PATH);
				char *f[] = {f_count,0};
				check_query = construct_query(f,from(ASOC_TABLE),where(cond(ASOC_PATH,"=",file_path)),0,0);
				int num = exec_int_scalarquery(check_query);
				if(!num){
					char *clean_query = NULL;
					clean_query = construct_delete(FILE_TABLE,where(cond(FILE_PATH,"=",file_path)));
					exec_nonquery(clean_query);
					free(clean_query);
				}
				free(f_count);free(file_path);
				free(check_query);
				free(delete);
				temp = strtok(NULL,",");
			}else{
				//Finalize 'cause can't select a path
				return;
			}
		}
		free(aux);
	}
}

/* See specification in task_functions.h */
void rm_task(char *filter){
	char *query = NULL;
	char *fields[] = {TASK_ID,0};
	if(filter!=NULL){
		query = construct_query(fields,from(TASK_TABLE),strdup(filter),0,0);
	}else{
		query = construct_query(fields,from(TASK_TABLE),0,0,0);
	}
	generic_list result = exec_query(query);
	if(result!=NULL){
		if(result->task_info!=NULL){
			task_list aux = result->task_info;
			//For each task to remove delete asociations and asignations to task
			while(aux!=NULL){
				printf("- Removing asignations and asociations to task %d ...\n",aux->task_id);
				rm_asociations_task(aux->task_id);
				rm_asignations_task(aux->task_id);
				printf("- Task %d prepared to be removed\n",aux->task_id);
				aux=aux->next;
			}
		}
	}
	dealloc(result);
	//Remove all tasks that matched with given filter
	char *delete = NULL;
	if(filter!=NULL){
		delete = construct_delete(TASK_TABLE,strdup(filter));
	}else{
		delete = construct_delete(TASK_TABLE,0);
	}
	exec_nonquery(delete);
	printf("** All selected task removed successfully\n");
	free(delete);free(filter);free(query);
}

/* See specification in task_functions.h */
char *filter_task(char *id,char *name,char *state,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time){
	//Format strings
	char *f_state = NULL; 
	char *u_state = NULL;
	if(strlen(state)>0 ) {f_state=format_string(state);u_state = upper(f_state);}
	char *f_est_start = NULL; if(strlen(est_start)>0) f_est_start=format_string(est_start); 
	char *f_est_end = NULL; if(strlen(est_end)>0) f_est_end=format_string(est_end); 
	char *f_start = NULL; if(strlen(start)>0) f_start=format_string(start); 
	char *f_end = NULL; if(strlen(end)>0) f_end=format_string(end); 
	char *u_prior = NULL;
	char *f_prior=NULL;if(strlen(prior)>0){ f_prior=format_string(prior);u_prior = upper(f_prior);} 
	char *u_type=NULL;
	char *f_type= NULL; if(strlen(type)>0){ f_type= format_string(type);u_type = upper(f_type);}
	//Construct query
	char *where_part = NULL;
	if((strlen(id)>0 || strlen(name)>0 || strlen(state)>0 ||
		strlen(est_start)>0 || strlen(est_end)>0 || strlen(start)>0 || strlen(end)>0 || strlen(prior)>0 ||
		strlen(type)>0 || strlen(est_time)>0 || strlen(time)>0)){
		if(strlen(id)>0){ 
			if(where_part==NULL) where_part = cond(TASK_ID,"=",id);
			else where_part=_and_(where_part,cond(TASK_ID,"=",id)); }
		if(u_state!=NULL){ if(where_part==NULL) where_part = cond(TASK_STATE,"=",u_state);
				else where_part=_and_(where_part,cond(TASK_STATE,"=",u_state));} 
		if(name!=NULL){
			if(where_part==NULL) where_part = contains(TASK_NAME,name);
				else where_part=_and_(where_part,contains(TASK_NAME,name)); }
		if(f_est_start!=NULL){ if(where_part==NULL) where_part = cond(TASK_START_EST,"=",f_est_start);
				else where_part=_and_(where_part,cond(TASK_START_EST,"=",f_est_start)); }
		if(f_est_end!=NULL){ if(where_part==NULL) where_part = cond(TASK_END_EST,"=",f_est_end);
				else where_part=_and_(where_part,cond(TASK_END_EST,"=",f_est_end));} 
		if(f_start!=NULL){ if(where_part==NULL) where_part = cond(TASK_START_REAL,"=",f_start);
				else where_part=_and_(where_part,cond(TASK_START_REAL,"=",f_start)); }
		if(f_end!=NULL){ if(where_part==NULL) where_part = cond(TASK_END_REAL,"=",f_end);
				else where_part=_and_(where_part,cond(TASK_END_REAL,"=",f_end));} 
		if(u_prior!=NULL){
				if(where_part==NULL) where_part = cond(TASK_PRIOR,"=",u_prior);
				else where_part=_and_(where_part,cond(TASK_PRIOR,"=",u_prior));} 
		if(u_type!=NULL){ if(where_part==NULL) where_part = cond(TASK_TYPE,"=",u_type);
				else where_part=_and_(where_part,cond(TASK_TYPE,"=",u_type));} 
		if(strlen(est_time)>1){ if(where_part==NULL) where_part = cond(TASK_APROXTIME,"=",est_time);
				else where_part=_and_(where_part,cond(TASK_APROXTIME,"=",est_time));} 
		if(strlen(time)>1){ if(where_part==NULL) where_part = cond(TASK_REALTIME,"=",time);
				else where_part=_and_(where_part,cond(TASK_REALTIME,"=",time));} 
	}
	char *where_final = NULL;
	if(where_part!=NULL) where_final = where(where_part); 
	//free temporal data
	if(u_state!=NULL) free(u_state); if(u_prior!=NULL) free(u_prior);
	if(u_type!=NULL) free(u_type); if(f_prior!=NULL) free(f_prior);
	if(f_state!=NULL) free(f_state);
	if(f_type!=NULL) free(f_type);if(f_est_start!=NULL) free(f_est_start);
	if(f_est_end!=NULL) free(f_est_end); if(f_start!=NULL) free(f_start);
	if(f_end!=NULL) free(f_end);
	return where_final;
}

/* See specification in task_functions.h */
void show_types(){
	char *query = NULL;
	char *fields[] = {ALL,0};
	query = construct_query(fields,from(TYPE_TABLE),NULL,NULL,NULL);
	//execute query	
	generic_list data = NULL;
	data = 	exec_query(query);
	
	free(query);
	//show data
	if(data == NULL){
		printf("No available task types yet");
	}else{
		type_list info = (*data).type_info;
		if(info==NULL){
			printf("No available task types yet");
		}else{
			printf("Available task types:\n");
			while(info!=NULL){
				printf("> %s\n",info->type_name);
				info=info->next;
			}
		}
	}
	dealloc(data);
}

/* See specification in task_functions.h */
void show_states(){
	char *query = NULL;
	char *fields[] = {ALL,0};
	query = construct_query(fields,from(STATE_TABLE),NULL,NULL,NULL);
	//execute query	
	generic_list data = NULL;
	data = 	exec_query(query);
	
	free(query);
	//show data
	if(data == NULL){
		printf("No available task states yet");
	}else{
		state_list info = (*data).state_info;
		if(info==NULL){
			printf("No available task states yet");
		}else{
			printf("Available task states:\n");
			while(info!=NULL){
				printf("> %s\n",info->state_name);
				info=info->next;
			}
		}
	}
	dealloc(data);
}

/* See specification in task_functions.h */
void show_priorities(){
	char *query = NULL;
	char *fields[] = {ALL,0};
	query = construct_query(fields,from(PRIOR_TABLE),NULL,NULL,NULL);
	//execute query	
	generic_list data = NULL;
	data = 	exec_query(query);
	
	free(query);
	//show data
	if(data == NULL){
		printf("No available task priorities yet");
	}else{
		prior_list info = (*data).prior_info;
		if(info==NULL){
			printf("No available task priorities yet");
		}else{
			printf("Available task priorities:\n");
			while(info!=NULL){
				printf("> %s\n",info->prior_name);
				info=info->next;
			}
		}
	}
	dealloc(data);
}

/* See specification in task_functions.h */
void show_pending(char *username){
	char *query = NULL;
	char *fields[] = {ASIG_TASK_ID,0};
	char *f_name = format_string(username);
	char *user_filter = where(cond(ASIG_USER_NAME,"=",f_name));
	query = construct_query(fields,from(ASIG_TABLE),user_filter,NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	if(data!=NULL){
		asig_list info = (*data).asig_info;
		if(info!=NULL){
			printf("These are pending tasks for %s :\n",username);
			while(info!=NULL){
				print_if_pending(info->task_id);
				info=info->next;
			}
		}else{
			printf("You haven't assigned tasks yet...\n");
		}
		dealloc(data);
	}else{
		printf("You haven't assigned tasks yet...\n");
	}
	
	if(f_name!=NULL) free(f_name);
	if(query!=NULL) free(query);
}

/* See specification in task_functions.h */
int select_action(char *id){
	FILE *temp_file=NULL;
	//Read record in file
	struct info *read_log = (struct info *) malloc(sizeof(struct info));
	temp_file = fopen(LOG_FILENAME,"rb");
	int result = -1;
	if(temp_file!=NULL){
		int r = fread(read_log,sizeof(struct info),1,temp_file);
		if(r!=0){
			if(read_log->tid == atoi(id)){
				//User wants to deactivate task previously activated
				result = SWITCH_SAME;
			}else{
				//User wants to change active working task to other
				result = SWITCH_OTHER;
			}
		}else{
			//If nothing in file then empty, no task activated
			result = SWITCH_EMPTY;
		}
		fclose(temp_file);
	}else{
		//If file doesn't exists then empty, no task activated
		result = SWITCH_EMPTY;
	}
	free(read_log);
	return result;
}

/* See specification in task_functions.h */
void activate_task(char *id){
	FILE *temp_file=NULL;
	struct info *write_log = (struct info *) malloc(sizeof(struct info));
	write_log->tid = atoi(id);
	write_log->init = time(NULL);
	temp_file = fopen(LOG_FILENAME,"wb");
	if(temp_file!=NULL){
		fwrite(write_log,sizeof(struct info),1,temp_file);
		fclose(temp_file);
		printf("Task %s activated...\n",id);
	}
	free(write_log);
}

/* See specification in task_functions.h */
void deactivate_task(char *username){
	FILE *temp_file=NULL;
	//Read record in file
	struct info *read_log = (struct info *) malloc(sizeof(struct info));
	temp_file = fopen(LOG_FILENAME,"rb");
	if(temp_file!=NULL){
		int r = fread(read_log,sizeof(struct info),1,temp_file);
			if(r!=0){
			char *insert = NULL;
			time_t end = time(NULL);
			char *f_uname = format_string(username);
			char *f_tid = format_number(read_log->tid);
			char *f_init = format_number((int)read_log->init);
			char *f_end = format_number((int)end);
			char *field_names[] = {LOG_USER_NAME,LOG_TASK_ID,LOG_START,LOG_END,0};
			char *field_data[] = {f_uname,f_tid,f_init,f_end,0};
			insert = construct_insert(LOG_TABLE,field_names,field_data);
			//Exec query
			exec_nonquery(insert);
			char *empty_field = (char *) malloc(1);empty_field[0]='\0';
			char *filter = filter_task(f_tid,empty_field,empty_field,empty_field,empty_field,
				empty_field,empty_field,empty_field,empty_field,empty_field,empty_field);
			float hours =  ((float) end - (float) read_log->init)/3600.0;
			char *f_hours = format_float(hours);
			update_task(filter,NULL,NULL,NULL,NULL,
					NULL,NULL,NULL,NULL,NULL,NULL,NULL,f_hours);
			//Free temporal data
			free(f_uname);free(f_tid);free(f_init);free(f_end);free(f_hours);free(empty_field);
			if(remove(LOG_FILENAME)==0){
				printf("Task %d deactivated ( %f hours spent )\n",read_log->tid,(hours));
			}
		}else{
			printf("Unexpected error deactivating previously activated task ...\n");
		}
		fclose(temp_file);
	}
	free(read_log);
}

/* See specification in task_functions.h */
void show_stats(){
	
	char *query = NULL;
	char *c = count(TASK_ID);
	char *fields[] = {c,0};
	
	//Obtaining total number of tasks
	query = construct_query(fields,from(TASK_TABLE),NULL,NULL,NULL);
	int total = 	exec_int_scalarquery(query);
	free(query);
	
	//Obtaining total number of new tasks
	query = construct_query(fields,from(TASK_TABLE),where(cond(TASK_STATE,"=","'NEW'")),NULL,NULL);
	int task_new = exec_int_scalarquery(query);
	free(query);
	
	//Obtaining total number of in progress tasks
	query = construct_query(fields,from(TASK_TABLE),where(cond(TASK_STATE,"=","'IN PROGRESS'")),NULL,NULL);
	int task_inprogress = exec_int_scalarquery(query);
	free(query);
	
	//Obtaining total number of resolved tasks
	query = construct_query(fields,from(TASK_TABLE),where(cond(TASK_STATE,"=","'RESOLVED'")),NULL,NULL);
	int task_resolved = exec_int_scalarquery(query);
	free(query);
	
	free(c);
	
	char *sum_approx = sum(TASK_APROXTIME);
	char *f_approx[] = {sum_approx,0};
	
	char *sum_real = sum(TASK_REALTIME);
	char *f_real[] = {sum_real,0};
	
	char *where_part = where(_or_(_or_(cond(TASK_STATE,"=","'NEW'"),cond(TASK_STATE,"=","'IN PROGRESS'")),cond(TASK_STATE,"=","'RESOLVED'")));
	
	//Obtaining total approx time inverted in project (with rejected tasks included)
	query = construct_query(f_approx,from(TASK_TABLE),NULL,NULL,NULL);
	float aprox_time_with = exec_float_scalarquery(query);
	free(query);
	
	//Obtaining total real time inverted in project (with rejected tasks included)
	query = construct_query(f_real,from(TASK_TABLE),NULL,NULL,NULL);
	float real_time_with = exec_float_scalarquery(query);
	free(query);
	
	//Obtaining total approx time inverted in project (WITHOUT rejected tasks included)
	query = construct_query(f_approx,from(TASK_TABLE),strdup(where_part),NULL,NULL);
	float aprox_time = exec_float_scalarquery(query);
	free(query);
	
	//Obtaining total real time inverted in project (WITHOUT rejected tasks included)
	query = construct_query(f_real,from(TASK_TABLE),strdup(where_part),NULL,NULL);
	float real_time = exec_float_scalarquery(query);
	free(query);
	
	free(where_part);
	free(sum_approx);
	free(sum_real);
	
	printf("*****************************************************\n");
	printf("               Project Statistics\n");
	printf("*****************************************************\n");
	//Number of tasks by state
	if(total>0){
		printf("Project task completion : %.2f %%\n",( ((float)task_resolved/(float)(task_new+task_inprogress+task_resolved))*100));
		printf("	Time including rejected tasks\n");
		printf("		- Estimated time : %.2f hours\n", aprox_time_with );
		printf("		- Real time      : %.2f hours\n", real_time_with );
		printf("	Time without rejected tasks\n");
		printf("		- Total estimated time : %.2f hours\n", aprox_time );
		printf("		- Total real time      : %.2f hours\n", real_time );
		printf("Total tasks: %d\n",total);
	
	
		char *s_field[] = {STATE_NAME,0};
		query = construct_query(s_field,from(STATE_TABLE),NULL,NULL,NULL);
		generic_list data = NULL;
		data = exec_query(query);
		if(data!=NULL){
			char *t_count = count(TASK_ID);
			char *f_state[] = {t_count,0};
			state_list info = (*data).state_info;
			printf("	Task number by state\n");
			while(info!=NULL){
				char *iter_query = construct_query(f_state,from(TASK_TABLE),where(cond(TASK_STATE,"=",format_string(info->state_name))),NULL,NULL);
				int by_state = exec_int_scalarquery(iter_query);
				free(iter_query);
				printf("		%s : %d (%.2f %%)\n",info->state_name,by_state,((float)by_state/(float)total)*100);
				info = info->next;
			}
			free(t_count);
			dealloc(data);
		}
		free(query);
		
		char *p_field[] = {PRIORITY_NAME,0};
		query = construct_query(p_field,from(PRIOR_TABLE),NULL,NULL,NULL);
		data = exec_query(query);
		if(data!=NULL){
			char *t_count = count(TASK_ID);
			char *f_prior[] = {t_count,0};
			prior_list info = (*data).prior_info;
			printf("	Task number by priority\n");
			while(info!=NULL){
				char *iter_query = construct_query(f_prior,from(TASK_TABLE),where(cond(TASK_PRIOR,"=",format_string(info->prior_name))),NULL,NULL);
				int by_prior = exec_int_scalarquery(iter_query);
				free(iter_query);
				printf("		%s : %d (%.2f %%)\n",info->prior_name,by_prior,((float)by_prior/(float)total)*100);
				info = info->next;
			}
			free(t_count);
			dealloc(data);
		}
		free(query);
		
		char *t_field[] = {TYPE_NAME,0};
		query = construct_query(t_field,from(TYPE_TABLE),NULL,NULL,NULL);
		data = exec_query(query);
		if(data!=NULL){
			char *t_count = count(TASK_ID);
			char *f_type[] = {t_count,0};
			type_list info = (*data).type_info;
			printf("	Task number by type\n");
			while(info!=NULL){
				char *iter_query = construct_query(f_type,from(TASK_TABLE),where(cond(TASK_TYPE,"=",format_string(info->type_name))),NULL,NULL);
				int by_type = exec_int_scalarquery(iter_query);
				free(iter_query);
				printf("		%s : %d (%.2f %%)\n",info->type_name,by_type,((float)by_type/(float)total)*100);
				info = info->next;
			}
			free(t_count);
			dealloc(data);
		}
		free(query);
		
		//Number of task assignations by user
		char *u_field[] = {USER_NAME,0};
		query = construct_query(u_field,from(USER_TABLE),NULL,NULL,NULL);
		data = exec_query(query);
		if(data!=NULL){
			char *asig_count = count(ASIG_TASK_ID);
			char *f_asig[] = {asig_count,0};
			user_list info = (*data).user_info;
			printf("	Task assignations by user\n");
			while(info!=NULL){
				char *iter_query = construct_query(f_asig,from(ASIG_TABLE),where(cond(ASIG_USER_NAME,"=",format_string(info->user_name))),NULL,NULL);
				int assigned = exec_int_scalarquery(iter_query);
				free(iter_query);
				printf("		%s : %d (%.2f %%)\n",info->user_name,assigned,((float)assigned/(float)total)*100);
				info = info->next;
			}
			free(asig_count);
			dealloc(data);
		}
		free(query);
		
		//Time spent and logged by user
		query = construct_query(u_field,from(USER_TABLE),NULL,NULL,NULL);
		data = exec_query(query);
		if(data!=NULL){
			char *l_asig[] = {LOG_START,LOG_END,0};
			user_list info = (*data).user_info;
			printf("	Time logged by user\n");
			while(info!=NULL){
				char *iter_query = construct_query(l_asig,from(LOG_TABLE),where(cond(LOG_USER_NAME,"=",format_string(info->user_name))),NULL,NULL);
				generic_list data2 = exec_query(iter_query);
				float logged_time = 0.0;
				if(data2!=NULL){
					log_list logs = (*data2).log_info;
					while(logs!=NULL){
						int start = atoi(logs->start);
						int end = atoi(logs->end);
						logged_time = logged_time+(((float)end-(float)start)/3600.0);
						logs = logs->next;
					} 
					dealloc(data2);
				}
				printf("		%s : %.2f hours\n",info->user_name,logged_time);
				info = info->next;
				free(iter_query);
			}
			dealloc(data);
		}
		free(query);
		
	}else{
		printf("Total tasks: 0\n");
	}
}
