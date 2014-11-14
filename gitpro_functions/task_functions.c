#include "task_functions.h"
#include "../gitpro_api/gitpro_data_api.h"
#include "../gitpro_api/db_constants.h"

/* Declarations to use in private functions */
void rm_assign_task(char *id,char *rm);
void rm_link_files(char *id,char *rm);

/*******************/
/*PRIVATE FUNCTIONS*/
/*******************/

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
void read_task(char *filter){
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
			printf("%d | Name: %s\tState: %s\tPriority: %s\tType: %s\n",aux->task_id,aux->task_name,aux->state,aux->priority,aux->type);
			printf("\tStart\tEstimated: %s\tReal: %s\n",aux->est_start_date,aux->start_date);
			printf("\tEnd  \tEstimated: %s\tReal: %s\n",aux->est_end_date,aux->end_date);
			printf("\tTime \tEstimated: %d\tReal: %d\n",aux->est_time_min,aux->real_time_min);
			printf("\tDescription: %s\n",aux->description);
			printf("\tNotes: %s\n",aux->notes);
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

	char *asignations[13];
	
	int i=0;

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
		asig_time=asig(TASK_REALTIME,f_time); 
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
	if(time!=NULL){free(f_time);}
	
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
	char *f_name =NULL; if(strlen(name)>0){ f_name =format_string(name);}
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
		if(f_name!=NULL){
			if(where_part==NULL) where_part = cond(TASK_NAME,"=",f_name);
				else where_part=_and_(where_part,cond(TASK_NAME,"=",f_name)); }
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
	if(f_name!=NULL) free(f_name); if(f_state!=NULL) free(f_state);
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
