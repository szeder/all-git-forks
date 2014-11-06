#include "v_codes.h"
#include "task_validate.h"
#include "../gitpro_api/gitpro_data_api.h"
#include "../gitpro_api/db_constants.h"
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#define PRIORITY 1
#define STATE 2
#define TYPE 3

#define NUMBER_SIZE 10

struct aux_file {
	char name[100]; //file name
	int n_paths; //number of possible paths searched
	char paths[50][PATH_MAX+1]; //possible paths of file name
	char final_path[PATH_MAX+1]; //final path selected
};

struct aux_file add_files[100]; //number of files

int start_end_flag = 0; //to know when fill start data and end data to check consistent on creation
int year1,year2;
int month1,month2;
int day1,day2;

/*******************/
/*PRIVATE FUNCTIONS*/
/*******************/

/*	Name		: validate_constant
	Parameters	: constant to validate and type of validate (PRIORITY, TYPE or STATE)
	Return		: INCORRECT_DATA if constant is invalid (not in db) or DATA_OK if constant is valid
	Used for	: validate priority, state and type of task */
int validate_constant(char *constant,int validate_type){
	if(constant==NULL) return INCORRECT_DATA;
	char *fields[2];
	char *from_part = NULL;
	char *where_part = NULL;
	char *f_const = format_string(constant);
	char *u_const = upper(f_const);
	if(validate_type==PRIORITY){
		fields[0] = PRIORITY_NAME;
		from_part = from(PRIOR_TABLE);
		where_part = where(cond(PRIORITY_NAME,"=",u_const));
	}else if(validate_type==STATE){
		fields[0] = STATE_NAME;
		from_part = from(STATE_TABLE);
		where_part = where(cond(STATE_NAME,"=",u_const));
	}else if(validate_type==TYPE){
		fields[0] = TYPE_NAME;
		from_part = from(TYPE_TABLE);
		where_part = where(cond(TYPE_NAME,"=",u_const));
	}else return INCORRECT_DATA;
	fields[1]=0;
	char *query = NULL;
	query = construct_query(fields,from_part,where_part,NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	int ok = 0;	
	if((*data).state_info!=NULL){
		if(strcmp((*data).state_info->state_name,null_data)!=0){
			ok =1;
		}
	}
	if((*data).prior_info!=NULL){
		if(strcmp((*data).prior_info->prior_name,null_data)!=0){
			ok =1;
		}
	}
	if((*data).type_info!=NULL){
		if(strcmp((*data).type_info->type_name,null_data)!=0){
			ok =1;
		}
	}
	free(f_const);free(u_const);
	free(query); 
	dealloc(data);	
	if(ok){
		return DATA_OK;
	}	
	return INCORRECT_DATA;
}

/* 	Name		: validate_number
	Parameters	: number to validate (max NUMBER_SIZE digits)
	Return		: INCORRECT_DATA if parameter isn't a number or DATA_OK in other case */
int validate_number(char *number){
	if(strlen(number)>NUMBER_SIZE) return INCORRECT_DATA;
	int i =0;
	int err=0;
	while(i< strlen(number) ){
		int ascii = (int) number[i];
		if(ascii<48 || ascii>57){
			err=1;
		}
		i++;
	}
	if(err) return INCORRECT_DATA;
	return DATA_OK;
}

/*	Name		: validate_date
	Parameters	: date to validate
	Return		: INCORRECT_DATA if date does not match following pattern: dd/mm/yyyy or 
				 day, month or year isn't valid. DATA_OK in other case 
				 This function fills day, month and year in global variables to check consistent datas when appropiated */
int validate_date(char *date){
	if(date==NULL) return INCORRECT_DATA;
	int n_day,n_year,n_month,sep,i;
	n_day = n_year = n_month = sep = i = 0;
	while(i<strlen(date)){
		int ascii = (int) date[i];
		//47 is '/'
		if(ascii==47){ 
			sep++;
		}
		//48 is '0' and 57 is '9'
		else if(ascii>47 && ascii<58){
			if(!sep) n_day++;
			if(sep==1) n_month++;
			if(sep==2) n_year++;
		}else{
			return INCORRECT_DATA;
		}
		i++;
	}
	if(sep!=2 || n_day!=2 || n_month!=2 || n_year!=4) return INCORRECT_DATA;
	int day, month,year;
	//Allocates memory to insert day, month and year (one by one)
	char *aux = (char *) malloc(2+1);
	aux[0]=date[0];aux[1]=date[1];aux[2]='\0';
	day = atoi(aux);
	aux[0]=date[3];aux[1]=date[4];
	month = atoi(aux);
	free(aux);
	aux = &date[6];
	year = atoi(aux);
	aux = NULL;
	if(start_end_flag){
		day1=day;
		month1=month;
		year1=year;
	}else{
		day2=day;
		month2=month;
		year2=year;
	}
	//Check day, month and year ranges
	if(month==1 || month==3 || month==5 || month==7 || month==8 || month==10 || month==12){
		if(day>0 && day<32) return DATA_OK;
	}else if(month==4 || month==6 || month==9 || month==11){
		if(day>0 && day<31) return DATA_OK;
	}else if(month==2){
		if(year%4 == 0){
			if(day>0 && day<30) return DATA_OK;
		}else{
			if(day>0 && day<29) return DATA_OK;
		}
	}
	return INCORRECT_DATA;
}

/*	Name		: validate_consistent_dates 
 * Parameters	: days, months and years to validate of each date
 * Return		: DATA_OK or INCORRECT_DATA */
 int validate_consistent_dates(int d1,int d2,int m1, int m2,int y1,int y2){
	 if(y1>y2){
		 return INCORRECT_DATA;
	 }else if(m1>m2){
		return INCORRECT_DATA;
	 }else if(d1>d2){
		 return INCORRECT_DATA;
	 }
	 return DATA_OK;
 }

/*	Name		: validate_time
	Parameters	: time to validate
	Return		: INCORRECT_DATA if time does not match following pattern: nnnnnnnnnn[.|,]dd
				 (where nnnnnnnnnn is integer part and dd is decimal part) or DATA_OK if matches*/
int validate_time(char *time){
	if(time==NULL) return INCORRECT_DATA;
	int i,n_int,n_dec,comma,err;
	i=n_int=n_dec=comma=err=0;
	while(i<strlen(time)){
		int ascii = (int) time[i];
		if(comma && ascii>47 && ascii<58){
			n_dec++;
		}else if(time[i]==46 || time[i]==44){
			//46 and 44 is '.' and ',' (allowed decimal separators)
			comma++;
		}else if(ascii>47 && ascii<58){
			n_int++;
		}else{
			err=1;
		}
		i++;
	}
	if(err) return INCORRECT_DATA;
	if(comma>1 || n_dec>2 || n_int>NUMBER_SIZE) return INCORRECT_DATA;
	if(atoi(time)==0) return INCORRECT_DATA;
	return DATA_OK;
}

/*	Name		: validate_duplicate_task
	Parameters	: name, state, type and prior
	Return		: INCORRECT_DATA if at least one param is null, DUPLICATE_TASK if task
				 with given params already exists or DATA_OK in other case */
int validate_duplicate_task(char *name,char *state,char *type,char *prior){
	if(name==NULL || state==NULL || type==NULL || prior==NULL){
		return INCORRECT_DATA;
	}
	char *fields[] = {TASK_NAME,TASK_STATE,TASK_PRIOR,TASK_TYPE,0};
	char *f_name = format_string(name);
	char *f_state = format_string(state);
	char *f_type = format_string(type);
	char *f_prior = format_string(prior);
	char *u_state = upper(f_state);
	char *u_type = upper(f_type);
	char *u_prior = upper(f_prior);
	char *query = NULL;
	query = construct_query(fields,from(TASK_TABLE),
					where(
						_and_(
							_and_(
								_and_(cond(TASK_NAME,"=",f_name),cond(TASK_STATE,"=",u_state)),
								cond(TASK_TYPE,"=",u_type)),
							cond(TASK_PRIOR,"=",u_prior)
						)
					)
					,NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	int duplicate = 0;	
	if((*data).task_info!=NULL){
		if(strcmp((*data).task_info->task_name,null_data)!=0){
			duplicate =1;
		}
	}
	free(f_name);free(f_type);free(f_state);free(f_prior);
	free(u_state);free(u_type);free(u_prior);	
	free(query); 
	dealloc(data);	
	if(duplicate){
		return DUPLICATE_TASK;
	}	
	return DATA_OK;
}

/*	Name		: validate_filters
	Parameters	: filters data
	Return		: DATA_OK or INCORRECT_DATA 
	Used to		: validate filters set by user */
int validate_filters(char *id,char *state,char *type,char *prior,char *est_time,char *time,
					char *est_start,char *est_end,char *start,char *end){
	int check=0;
	if(strlen(id)>0){
		check = validate_number(id);
		if(check!=DATA_OK) return check;}
	if(strlen(state)>0){
		check = validate_constant(state,STATE);
		if(check!=DATA_OK) return check;}
	if(strlen(type)>0){
		check = validate_constant(type,TYPE);
		if(check!=DATA_OK) return check;}
	if(strlen(prior)>0){
		check = validate_constant(prior,PRIORITY);
		if(check!=DATA_OK) return check;}
	if(strlen(est_time)>0){
		check = validate_time(est_time);
		if(check!=DATA_OK) return check;}
	if(strlen(time)>0){
		check = validate_time(time);
		if(check!=DATA_OK) return check;}
	if(strlen(est_start)>0){
		check = validate_date(est_start);
		if(check!=DATA_OK) return check;}
	if(strlen(est_end)>0){
		check = validate_date(est_end);
		if(check!=DATA_OK) return check;}
	if(strlen(start)>0){
		check = validate_date(start);
		if(check!=DATA_OK) return check;}
	if(strlen(end)>0){
		check = validate_date(end);
		if(check!=DATA_OK) return check;}
	return DATA_OK;
}

/*	Name		: validate_users
	Parameters	: users to add or remove
	Return		: DATA_OK if data is valid or INEXISTENT_USER if some user does not exist */
int validate_users_task(char *users){
	if(users==NULL) return DATA_OK;
	char *aux = strdup(users);
	char *temp = strtok(aux,",");
	while(temp!=NULL){
		char *query = NULL;
		char *formatted_name = format_string(temp);
		char *fields[] = {USER_NAME,0};
		query = construct_query(fields,from(USER_TABLE),
						where(cond(USER_NAME,"=",formatted_name)),NULL,NULL);
		generic_list data = NULL;
		data = exec_query(query);
		int inex = 0;
		if((*data).user_info==NULL){
			inex = 1;
		}else{
			if(strcmp((*data).user_info->user_name,null_data)==0){
				inex=1;
			}
		}
		free(formatted_name);
		free(query);
		dealloc(data);
		if(inex){
			free(aux);
			return INEXISTENT_USER;
		}
		temp = strtok(NULL,",");
	}
	free(aux);
	return DATA_OK;
}

/*	Name		: validate_inexistent_task
	Parameters	: task id
	Return		: DATA_OK if data is valid, INCORRECT_DATA if id is not specified
				 or INEXISTENT_TASK if task id does not exists */
int validate_inexistent_task(char *id){
	if(id==NULL) return INCORRECT_DATA;
	char *fields[] = {TASK_ID,0};
	char *query = NULL;
	query = construct_query(fields,from(TASK_TABLE),
					where(cond(TASK_ID,"=",id)),NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	int ok = 0;
	if((*data).task_info!=NULL){
		if((*data).task_info->task_id!=-1){
			ok=1;
		}
	}
	free(query);
	dealloc(data);
	if(ok) return DATA_OK;
	return INEXISTENT_TASK;
}

/*	Name		: parse_add_files
	Parameters	: files to add
	Return		: number of files
	Used for	: fill add_files struct with file info */
int parse_add_files(char *add){
	char *aux = strdup(add);
	char *temp = strtok(aux,",");
	int i=0;
	while(temp!=NULL){
		strcpy(add_files[i].name,temp);
		add_files[i].n_paths=0;
		i++;
		temp = strtok(NULL,",");
	}
	free(aux);
	return i;
}

/*	Name		: rec_search_add_files
	Parameters	: files to link, full path of name directory (NULL if first call,
				 following calls built path internal)
	Return		: DATA_OK and file data inserted if all files are in repository. INEXISTENT_FILE_FOLDER
				 if at least one file doesn't exists in repository */
int rec_search_add_files(struct aux_file add[],int n_files,char *full_path){
	DIR *d;
	struct dirent *dir;
	char actual_full_path[PATH_MAX+1];
	if(full_path==NULL){
		d = opendir(".");
	}else{
		d = opendir(full_path);
	}
	if(d){
		while((dir=readdir(d))!=NULL){
			char *name = dir->d_name;
			char real_name[PATH_MAX+1];
			if(full_path==NULL){
				char path[PATH_MAX+1];
				char *ptr = NULL;
				ptr = realpath(name,path);
				strcpy(actual_full_path,ptr);
				strcpy(real_name,actual_full_path);
			}else{
				strcpy(actual_full_path,full_path);
				strcpy(real_name,actual_full_path);
				strcat(real_name,"/");
				strcat(real_name,name);
			}
			if(strcmp(name,".") && strcmp(name,"..")){
				struct stat s;
				int err = stat(real_name,&s);
				if(err==-1){
					//printf("not exists %s\n",real_name);
				}else{
					int i=0;
					while(i<n_files){
						if(!strcmp(name,add[i].name)){
							strcpy(add[i].paths[add[i].n_paths],real_name);
							add[i].n_paths++;
						}
						i++;
					}
					if( (s.st_mode & S_IFMT) == S_IFREG){
						//printf("file %s\n",real_name);
					}else if( (s.st_mode & S_IFMT) == S_IFDIR){
						//printf("directory %s\n",real_name);
						rec_search_add_files(add,n_files,real_name);
					}
				}
			}
		}
		closedir(d);
	}
	return DATA_OK;
}

/*	Name		: select_ambig_paths
	Parameters	: number of files
	Return		: INEXISTENT_FILE_FOLDER if at least one not exists or DATA_OK 
	Used for	: resolve ambig paths of files or folders and insert data in db if
				 all data is ok */
int select_ambig_paths(int number){
	int i=0;
	int flag = 0;
	char home_path[1024];
	getcwd(home_path,sizeof(home_path));
	int skip_home_path = strlen(home_path); //Number of characters to be skipped (user home path part to be removed)
	while(i<number){
		if(!add_files[i].n_paths){
			printf("%s does not exists...\n",add_files[i].name);
			flag=1;
		}else if(add_files[i].n_paths==1){
			strcpy(add_files[i].final_path,add_files[i].paths[0]+skip_home_path);
		}else{
			printf("Has found more than one path for file or folder '%s'\n",add_files[i].name);
			printf("Select one [0 - %d] and press ENTER\n",add_files[i].n_paths-1);
			int j=0;
			while(j<add_files[i].n_paths){
				printf("%d | %s\n",j,add_files[i].paths[j]+skip_home_path);
				j++;
			}
			char *option = (char *) malloc(NUMBER_SIZE);
			fgets(option,NUMBER_SIZE,stdin);
			option[strlen(option)-1]='\0';
			if(validate_number(option)!=DATA_OK) flag=1;
			else{
				int select = atoi(option);
				if(select<0 || select>=add_files[i].n_paths){
					free(option);
					return INCORRECT_DATA;
				}
				strcpy(add_files[i].final_path,add_files[i].paths[select]+skip_home_path);
			}
			free(option);
		}
		i++;
	}
	if(flag) return INCORRECT_DATA;
	else{
		i=0;
		while(i<number){
			char *query = NULL;
			char *path = format_string(add_files[i].final_path);
			char *name = format_string(add_files[i].name);
			char *fields[] = {FILE_NAME,FILE_PATH,0};
			char *values[] = {name,path,0};
			query = construct_insert(FILE_TABLE,fields,values);
			exec_nonquery(query);
			free(path);
			free(name);
			free(query);
			i++;
		}
	}
	return DATA_OK;
}

/*	Name		: validate_add_files
	Parameters	: files to add
	Return		: DATA_OK or INCORRECT_FILE_FOLDER */
int validate_add_files(char *add){
	if(add==NULL) return DATA_OK;
	int number_of_files = parse_add_files(add);
	int check = rec_search_add_files(add_files,number_of_files,NULL);
	if(check!=DATA_OK) return check;
	return select_ambig_paths(number_of_files);
}

/*	Name		: validate_rm_files
	Parameters	: files to link
	Return		: DATA_OK if all files exists in persistent data or INEXISTENT_FILE_FOLDER
				 if at least one file doesn't exists in persistent data */
int validate_rm_files(char *rm){
	if(rm==NULL) return DATA_OK;
	char *aux = strdup(rm);
	char *temp = strtok(aux,",");
	while(temp!=NULL){
		char *query = NULL;
		char *formatted_name = format_string(temp);
		char *fields[] = {FILE_PATH,0};
		query = construct_query(fields,from(FILE_TABLE),
						where(cond(FILE_NAME,"=",formatted_name)),NULL,NULL);
		generic_list data = NULL;
		data = exec_query(query);
		int inex = 0;
		if((*data).file_info==NULL){
			inex = 1;
		}else{
			if(strcmp((*data).file_info->file_path,null_data)==0){
				inex=1;
			}
		}
		free(formatted_name);
		free(query);
		dealloc(data);
		if(inex){
			free(aux);
			return INEXISTENT_FILE_FOLDER;
		}
		temp = strtok(NULL,",");
	}
	free(aux);
	return DATA_OK;
}

/*******************/
/* PUBLIC FUNCTIONS*/
/*******************/

/* See specification in task_validate.h */
int validate_create_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm){
	if(id!=NULL) return INCORRECT_DATA;
	if(add!=NULL || rm != NULL) return INCORRECT_DATA;
	if(name==NULL || state==NULL || prior==NULL || type==NULL) return INCORRECT_DATA;
	int dup = validate_duplicate_task(name,state,type,prior);
	if(dup!=DATA_OK) return DUPLICATE_TASK;
	int check = 0;
	check = validate_constant(state,STATE);
	if(check!=DATA_OK) return INCORRECT_DATA;
	check = validate_constant(prior,PRIORITY);
	if(check!=DATA_OK) return INCORRECT_DATA;
	check = validate_constant(type,TYPE);
	if(check!=DATA_OK) return INCORRECT_DATA;
	if(est_time!=NULL){
		check = validate_time(est_time);
		if(check!=DATA_OK) return check;
	}
	if(time!=NULL){
		check = validate_time(time);
		if(check!=DATA_OK) return check;
	}
	start_end_flag=1;
	if(est_start!=NULL){
		check = validate_date(est_start);
		if(check!=DATA_OK) return check;
	}
	start_end_flag=0;
	if(est_end!=NULL){
		check = validate_date(est_end);
		if(check!=DATA_OK) return check;
	}
	if(est_start!=NULL && est_end!=NULL){
		check = validate_consistent_dates(day1,day2,month1,month2,year1,year2);
		if(check!=DATA_OK) return check;
	}
	start_end_flag=1;
	if(start!=NULL){
		check = validate_date(start);
		if(check!=DATA_OK) return check;
	}
	start_end_flag=0;
	if(end!=NULL){
		check = validate_date(end);
		if(check!=DATA_OK) return check;
	}
	if(start!=NULL && end!=NULL){
		check = validate_consistent_dates(day1,day2,month1,month2,year1,year2);
		if(check!=DATA_OK) return check;
	}
	return DATA_OK;					
} 
 
/* See specification in task_validate.h */
int validate_delete_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm){
	if(add!=NULL || rm!=NULL) return INCORRECT_DATA;
	if(desc!=NULL || notes!=NULL) return INCORRECT_DATA;
	return validate_filters(id,state,type,prior,est_time,time,est_start,est_end,start,end);
}

/* See specification in task_validate.h */
int validate_update_task(int are_filters,char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm){
	if(add!=NULL || rm!=NULL) return INCORRECT_DATA;
	if(!are_filters && id!=NULL) return INCORRECT_DATA;
	if(are_filters){
		return validate_filters(id,state,type,prior,est_time,time,est_start,est_end,start,end);
	}else{
		int check=0;
		if(state!=NULL){
			check = validate_constant(state,STATE);
			if(check!=DATA_OK) return check;
		}
		if(type!=NULL){
			check = validate_constant(type,TYPE);
			if(check!=DATA_OK) return check;
		}
		if(prior!=NULL){
			check = validate_constant(prior,PRIORITY);
			if(check!=DATA_OK) return check;
		}
		if(est_time!=NULL){
			check = validate_time(est_time);
			if(check!=DATA_OK) return check;
		}
		if(time!=NULL){
			check = validate_time(time);
			if(check!=DATA_OK) return check;
		}
		if(est_start!=NULL){
			check = validate_date(est_start);
			if(check!=DATA_OK) return check;
		}
		if(est_end!=NULL){
			check = validate_date(est_end);
			if(check!=DATA_OK) return check;
		}
		if(start!=NULL){
			check = validate_date(start);
			if(check!=DATA_OK) return check;
		}
		if(end!=NULL){
			check = validate_date(end);
			if(check!=DATA_OK) return check;
		}
	}
	return DATA_OK;
}

/* See specification in task_validate.h */
int validate_read_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm){
	if(add!=NULL || rm!=NULL) return INCORRECT_DATA;
	if(desc!=NULL || notes!=NULL) return INCORRECT_DATA;
	return validate_filters(id,state,type,prior,est_time,time,est_start,est_end,start,end);				
}

/* See specification in task_validate.h */
int validate_assign_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm){
	if(name!=NULL || state!=NULL || desc!=NULL || notes!=NULL || est_start!=NULL || est_end!=NULL ||
		start!=NULL || end!=NULL || prior!=NULL || type!=NULL || est_time!=NULL || time!=NULL){
		return INCORRECT_DATA;
	}
	if(id==NULL) return INCORRECT_DATA;
	if(add==NULL && rm==NULL) return INCORRECT_DATA;
	int check=0;
	if(add!=NULL){
		check=validate_users_task(add);
		if(check!=DATA_OK) return check;
	}
	if(rm!=NULL){
		check=validate_users_task(rm);
		if(check!=DATA_OK) return check;
	}
	check=validate_inexistent_task(id);
	if(check!=DATA_OK) return check;
	return DATA_OK;					
}
					
/* See specification in task_validate.h */
int validate_link_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm){
	if(name!=NULL || state!=NULL || desc!=NULL || notes!=NULL || est_start!=NULL || est_end!=NULL ||
		start!=NULL || end!=NULL || prior!=NULL || type!=NULL || est_time!=NULL || time!=NULL){
		return INCORRECT_DATA;
	}
	if(id==NULL) return INCORRECT_DATA;
	if(add==NULL && rm==NULL) return INCORRECT_DATA;
	int check=0;
	if(rm!=NULL){
		check=validate_rm_files(rm);
		if(check!=DATA_OK) return check;
	}
	check=validate_inexistent_task(id);
	if(check!=DATA_OK) return check;
	if(add!=NULL){
		check=validate_add_files(add);
		if(check!=DATA_OK) return check;
	}
	return DATA_OK;					
}
