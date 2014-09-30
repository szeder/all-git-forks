#include "gitpro_data_api.h"

static sqlite3 *db_ptr; //pointer to sqlite3 database
int int_scalar_data = -1; //int scalar obtained 
float float_scalar_data = -1; //float scalar obtained

/*******************/
/*PRIVATE FUNCTIONS*/
/*******************/

/* 	Name		: open_db
	Parameters	: nothing
	Return		: 1 if success of 0 in other case */
int open_db(){
	if(sqlite3_open(DB_NAME,&db_ptr)==SQLITE_OK){
		return 1;
	}else{
		return 0;
	}
}

/* 	Name		: close_db
	Parameters	: nothing
	Return		: 1 if success or 0 in other case */
int close_db(){
	if(sqlite3_close(db_ptr)==SQLITE_OK){
		return 1;	
	}else{
		return 0;
	}
}

/* 	Name		: nonquery_callback
	Parameters	: personalized data (not used in this case), number of cols (a), 
				 data returned (b) and col names (c). any of that parameters
				 is used in this case
	Return		: 0 
	Note		: It's executed automatically by each row in result (not apply in this case) */
static int nonquery_callback(void *not_used,int a,char **b,char **c){
	return 0;
}

/* 	Name		: int_scalar_query_callback
	Parameters	: number of cols, data returned 
				 and col names.
	Return		: 0
	Used for	: fill int scalar data to be returned in specific function call
				 (exec_int_scalarquery )
	Note		: it's executed automatically by each row in result. */
static int int_scalar_query_callback(void *notUsed,int ncol,char **col_data,char **col_name){
	if(col_data[0]!=NULL) int_scalar_data = atoi(col_data[0]);
	return 0;
}

/* 	Name		: float_scalar_query_callback
	Parameters	: number of cols, data returned 
				 and col names.
	Return		: 0
	Used for	: fill float scalar data to be returned in specific function call
				 ( exec_float_scalar_query)
	Note		: it's executed automatically by each row in result. */
static int float_scalar_query_callback(void *notUsed,int ncol,char **col_data,char **col_name){
	if(col_data[0]!=NULL) float_scalar_data = atof(col_data[0]);	
	return 0;
}

/*	Name		: query_callback
	Parameters	: personalized data (not used in this case), number of cols, 
				 col data of row and col names
	Return		: 0
	Used for	: fill generic list that will be returned with query result data
	Note		: it's executed automatically by each row in result */
static int query_callback(void *not_used,int ncol,char **col_data, char **col_name){	
	//Temporal variables to fill role data	
	char *role_name = NULL;
	int cr,ar,ur,rr,lf,rt,at,ut,dt,ct;
	cr = ar = ur = rr = lf = rt = at = ut = dt = ct = -1;
	//Temporal variables to fill task data
	char *task_name,*state,*desc,*notes,*prior,*type;
	char *est_start_date,*est_end_date,*start_date,*end_date;
	int id,real_time,est_time;
	task_name = state = desc = notes = prior = type = NULL;
	est_start_date = est_end_date = start_date = end_date = NULL;
	id = real_time = est_time = -1;
	//Temporal variables to fill user data
	char *user_name,*user_role;
	user_name = user_role = NULL;
	//Temporal variables to fill file data
	char *file_name,*file_path;
	file_name = file_path = NULL;
	//Temporal variables to fill asociation data
	char *asoc_path = NULL; int asoc_tid = -1;
	//Temporal variables to fill asignation data
	char *asig_user = NULL; int asig_tid = -1;
	//Temporal variables to fill state data
	char *state_name=NULL;
	//Temporal variables to fill prior data
	char *prior_name=NULL;
	//Temporal variables to fill type data
	char *type_name=NULL;
	//Count variable	
	int i;	
	for(i=0;i<ncol;i++){
		if(col_data[i]!=NULL){
			if(!strcmp(col_name[i],ROLE_NAME)){
				role_name = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],CREATE_ROLE)){
				cr = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],ASSIGN_ROLE)){
				ar = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],UPDATE_ROLE)){
				ur = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],REMOVE_ROLE)){
				rr =atoi(col_data[i]);
			}else if(!strcmp(col_name[i],LINK_FILES)){
				lf = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],READ_TASK)){
				rt = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],ASSIGN_TASK)){
				at = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],UPDATE_TASK)){
				ut = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],REMOVE_TASK)){
				dt = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],CREATE_TASK)){
				ct = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_ID)){
				id = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_NAME)){
				task_name = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_STATE)){
				state = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_DESCRIP)){
				desc = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_NOTES)){
				notes =strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_PRIOR)){
				prior = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_TYPE)){
				type = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_REALTIME)){
				real_time = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_APROXTIME)){
				est_time = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],USER_NAME)){
				user_name = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],USER_ROLE)){
				user_role = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],FILE_NAME)){
				file_name = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],FILE_PATH)){
				file_path = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],ASOC_PATH)){
				asoc_path = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],ASOC_TASK_ID)){
				asoc_tid = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],ASIG_USER_NAME)){
				asig_user = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],ASIG_TASK_ID)){
				asig_tid = atoi(col_data[i]);
			}else if(!strcmp(col_name[i],TYPE_NAME)){
				type_name = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],STATE_NAME)){
				state_name = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],PRIORITY_NAME)){
				prior_name = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_START_EST)){
				est_start_date = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_END_EST)){
				est_end_date = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_START_REAL)){
				start_date = strdup(col_data[i]);
			}else if(!strcmp(col_name[i],TASK_END_REAL)){
				end_date = strdup(col_data[i]);
			}
		}
	}
	//Insert data on each list to return if at least one field has data
	if(role_name!=NULL || cr!=-1 || ar!=-1 || rt!=-1 || at!=-1 || ut!=-1 || lf!=-1 ||
		dt!=-1 || ct!=-1 || rr!=-1 || ur!=-1){
		if(role_name==NULL) role_name = strdup(null_data);
		insert_role(&roles,role_name,cr,ar,rt,at,ut,lf,dt,ct,rr,ur);
	}
	if(id!=-1 || task_name!=NULL || state!=NULL || desc!=NULL || notes!=NULL ||
		type!=NULL || prior!=NULL || real_time!=-1 || est_time!=-1){
		if(task_name==NULL) task_name = strdup(null_data);
		if(state==NULL) state = strdup(null_data);
		if(desc==NULL) desc = strdup(null_data);
		if(notes==NULL) notes = strdup(null_data);
		if(type==NULL) type = strdup(null_data);
		if(prior==NULL) prior = strdup(null_data);
		if(est_start_date==NULL) est_start_date = strdup(null_data);
		if(est_end_date==NULL) est_end_date = strdup(null_data);
		if(start_date==NULL) start_date = strdup(null_data);
		if(end_date==NULL) end_date = strdup(null_data);
		insert_task(&tasks,id,task_name,state,desc,notes,type,prior,real_time,est_time,est_start_date,est_end_date,start_date,end_date);	
	}
	if(user_name!=NULL || user_role!=NULL){
		if(user_name==NULL) user_name = strdup(null_data);
		if(user_role==NULL) user_role = strdup(null_data);
		insert_user(&users,user_name,user_role);	
	}
	if(file_name!=NULL || file_path!=NULL){
		if(file_name==NULL) file_name = strdup(null_data);
		if(file_path==NULL) file_path = strdup(null_data);
		insert_file(&files,file_name,file_path);
	}
	if(asoc_path!=NULL || asoc_tid!=-1){
		if(asoc_path==NULL) asoc_path = strdup(null_data);
		insert_asoc(&asocs,asoc_path,asoc_tid);	
	}		
	if(asig_user!=NULL || asig_tid!=-1){
		if(asig_user==NULL) asig_user = strdup(null_data);
		insert_asig(&asigs,asig_user,asig_tid);
	}
	if(state_name!=NULL){
		insert_state(&states,state_name);
	}
	if(prior_name!=NULL){
		insert_prior(&priors,prior_name);
	}
	if(type_name!=NULL){
		insert_type(&types,type_name);
	}
	//Free all temporal data	
	free(role_name);free(task_name);free(state);
	free(desc);free(notes);free(prior);free(type);
	free(user_name);free(file_name);free(file_path);
	free(user_role);free(asoc_path);free(asig_user);
	free(state_name);free(type_name);free(prior_name);	
	return 0;
}

/* 	Name 		: run_query
	Parameters	: sql query
	Return		: 1 if ok 0 in other case
	Used for	: encapsulate steps of query execution */
int run_query(const char *sql,int (*callback)(void*,int,char**,char**),void *aux){
	open_db();
	char *err_msg = 0;
	int check = sqlite3_exec(db_ptr,sql,callback,0,&err_msg);
	close_db();
	return check;
}


/*******************/
/* PUBLIC FUNCTIONS*/
/*******************/

/* See specification in gitpro_data_api.h */
int exec_nonquery(const char *sql){
	int check = run_query(sql,nonquery_callback,0);
	if(check){
		return 0;
	}
	return 1;
}

/* See specification in gitpro_data_api.h */
int exec_int_scalarquery(const char *sql){
	run_query(sql,int_scalar_query_callback,0);
	return int_scalar_data; 
}

/* See specification in gitpro_data_api.h */
float exec_float_scalarquery(const char *sql){
	run_query(sql,float_scalar_query_callback,0);
	return float_scalar_data; 
}

/* See specification in gitpro_data_api.h */
generic_list exec_query(const char *sql){
	run_query(sql,query_callback,0);
	generic = (generic_list) malloc(sizeof(struct generic_node));
	(*generic).role_info = roles;
	(*generic).task_info = tasks;
	(*generic).user_info = users;
	(*generic).file_info = files;
	(*generic).asoc_info = asocs;
	(*generic).asig_info = asigs;
	(*generic).type_info = types;
	(*generic).state_info = states;
	(*generic).prior_info = priors;
	roles=NULL;tasks=NULL;users=NULL;files=NULL;asocs=NULL;asigs=NULL;
	types=NULL;states=NULL;priors=NULL;
	return generic;
}

/* See specification in gitpro_data_api.h */
void dealloc(generic_list list){
	dealloc_roles((*list).role_info);
	dealloc_tasks((*list).task_info);
	dealloc_users((*list).user_info);
	dealloc_files((*list).file_info);
	dealloc_asocs((*list).asoc_info);
	dealloc_asigs((*list).asig_info);
	dealloc_priors((*list).prior_info);
	dealloc_types((*list).type_info);
	dealloc_states((*list).state_info);
	roles = NULL;
	tasks = NULL;
	users = NULL;
	asocs = NULL;
	files = NULL;
	asigs = NULL;
	priors = NULL;
	types = NULL;
	states = NULL;
	free(list);
	generic = NULL;
}

/* See specification in gitpro_data_api.h */
void show_roles(role_list list){
	printf("ROLE DATA\n");
	while(list!=NULL){
		printf("%s %d %d %d %d %d %d %d %d %d %d\n",(*list).role_name,(*list).create_role,
			(*list).assign_role,(*list).read_task,(*list).assign_task,(*list).update_task,
			(*list).link_files,(*list).remove_task,(*list).create_task,(*list).remove_role,
			(*list).update_role);
		list = (*list).next;
	}
}

/* See specification in gitpro_data_api.h */
void show_tasks(task_list list){
	printf("TASK DATA\n");
	while(list!=NULL){
		printf("%d %s %s %s %s %s %s %d %d\n",(*list).task_id,(*list).task_name,(*list).state,
			(*list).description,(*list).notes,(*list).type,(*list).priority,(*list).real_time_min,(*list).est_time_min);
		list = (*list).next;
	}
}

/* See specification in gitpro_data_api.h */
void show_users(user_list list){
	printf("USER DATA\n");
	while(list!=NULL){
		printf("%s %s\n",(*list).user_name,(*list).user_role);
		list = (*list).next;
	}
}

/* See specification in gitpro_data_api.h */
void show_files(file_list list){
	printf("FILE DATA\n");
	while(list!=NULL){
		printf("%s %s\n",(*list).file_name,(*list).file_path);
		list = (*list).next;
	}
}

/* See specification in gitpro_data_api.h */
void show_asocs(asoc_list list){
	printf("ASOCIATION DATA\n");
	while(list!=NULL){
		printf("%s %d\n",(*list).file_path,(*list).task_id);
		list = (*list).next;
	}
}

/* See specification in gitpro_data_api.h */
void show_asigs(asig_list list){
	printf("ASIGNATION DATA\n");
	while(list!=NULL){
		printf("%s %d\n",(*list).user_name,(*list).task_id);
		list = (*list).next;
	}
}
