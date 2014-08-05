#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "check_role.h"
#include "../gitpro_api/gitpro_data_api.h"
#include "../gitpro_api/db_constants.h"
#include "builtin.h"

#define HOME "HOME"
#define GLOBAL_FILE ".gitconfig"
#define NAME "name"

#define ADMIN_FILEPATH "/etc/.gitadmin"
#define ADMIN "admin"

/*******************/
/*PRIVATE FUNCTIONS*/
/*******************/

/*	Name		: get_role_result
	Parameters	: action to check and role name
	Return		: generic list with result data
	Used for	: don't duplicate code in each function of module */
generic_list get_role_result(char *perm,char *role_name){
	char *query = NULL;
	char *f_param = NULL;
	char *fields[] = {perm,0};
	f_param = format_string(role_name);
	query = construct_query(fields,from(ROLE_TABLE),where(cond(ROLE_NAME,"=",f_param)),
							NULL,NULL);	
	generic_list result = exec_query(query);
	free(f_param);
	free(query);
	return result;
}

/*******************/
/* PUBLIC FUNCTIONS*/
/*******************/

int is_admin(char *username){
	int result = 0;
	FILE *f = fopen(ADMIN_FILEPATH,"r");
	if(f!=NULL){
		int max = 100;
		char line[max];	
		char *end = NULL;	
		end = fgets(line,max,f);
		while(end!=NULL){
			char *name = strstr(line,ADMIN);
			if(name!=NULL){
				/* Calculate number of characters to skip */
				int i = strlen(ADMIN);
				while(name[i]==' ' || name[i]=='='){
					i++;
				}
				if(name!=NULL && !result){
					name[strlen(name)-1]='\0';
					/* If given name is equal to name in file then user is an admin */
					if(strcmp(username,name+i)==0){
						result = 1;
					}
				}
			}
			end = fgets(line,max,f);			
		}
		fclose(f);
	}	
	return result;
}

/* See specification in check_role.h */
char *get_role(char *username){
	char *fields[] = {USER_ROLE,0};
	char *formatted_name = format_string(username);
	char *query = construct_query(fields,from(USER_TABLE),
						where(cond(USER_NAME,"=",formatted_name)),NULL,NULL);
	generic_list data = exec_query(query);
	if((*data).user_info!=NULL){
		char *role = (char *) malloc(strlen((*data).user_info->user_role)+1);
		strcpy(role,(*data).user_info->user_role);		
		return role;
	}
	free(formatted_name);
	free(query);
	dealloc(data);
	return NULL;
}

/* See specification in check_role.h */
char *get_username(){
	char *home = getenv(HOME);
	char *result = NULL;
	if(home==NULL){
		fputs("No environment variable HOME defined\n",stderr);
	}else{
		/* Allocates memory to home path, 1 parenthesis, config file name and 
			end string character */
		char *path = (char *) malloc(strlen(home)+1+strlen(GLOBAL_FILE)+1);
		strcpy(path,home);
		strcat(strcat(path,"/"),GLOBAL_FILE);
		FILE *f = fopen(path,"r");
		free(path);
		if(f!=NULL){
			int max = 100;
			char line[max];	
			char *end = NULL;	
			end = fgets(line,max,f);
			while(end!=NULL){
				char *name = strstr(line,NAME);
				if(name!=NULL && result==NULL){
					/* Calculate number of characters to skip */						
					int i = strlen(NAME);
					while(name[i]==' ' || name[i]=='='){
						i++;
					}
					if(name!=NULL){
						name[strlen(name)-1]='\0';
						result = (char *) malloc(strlen(name+i)+1);
						/* Copy in result name moved 'i' times */	
						strcpy(result,name+i);
					}
				}
				end = fgets(line,max,f);			
			}
			fclose(f);		
		}
	}
	return result;
}

/* See specification in check_role.h */
int can_create_role(char *role_name){
	generic_list result = get_role_result(CREATE_ROLE,role_name);
	int value = -1;	
	if((*result).role_info!=NULL){
		value = (*result).role_info->create_role;	
	}
	dealloc(result);
	return value;
}

/* See specification in check_role.h */
int can_assign_role(char *role_name){
	generic_list result = get_role_result(ASSIGN_ROLE,role_name);
	int value = -1; 
	if((*result).role_info!=NULL){	
		return (*result).role_info->assign_role;
	}
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_read_task(char *role_name){
	generic_list result = get_role_result(READ_TASK,role_name);
	int value = -1;
	if((*result).role_info!=NULL){
	 	value = (*result).role_info->read_task;
	}
	dealloc(result);	
	return value;	
}

/* See specification in check_role.h */
int can_assign_task(char *role_name){
	generic_list result = get_role_result(ASSIGN_TASK,role_name);
	int value = -1;
	if((*result).role_info!=NULL){
	 	value = (*result).role_info->assign_task;
	}
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_update_task(char *role_name){
	generic_list result = get_role_result(UPDATE_TASK,role_name);
	int value = -1;
	if((*result).role_info!=NULL){	
		value = (*result).role_info->update_task;
	}
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_link_files(char *role_name){
	generic_list result = get_role_result(LINK_FILES,role_name);
	int value = -1;
	if((*result).role_info!=NULL){ 		
		value = (*result).role_info->link_files;
	}	
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_remove_task(char *role_name){
	generic_list result = get_role_result(REMOVE_TASK,role_name);
	int value = -1;
	if((*result).role_info!=NULL){
	 	value = (*result).role_info->remove_task;
	}
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_create_task(char *role_name){
	generic_list result = get_role_result(CREATE_TASK,role_name);
	int value = -1;
	if((*result).role_info!=NULL){
 		value = (*result).role_info->create_task;
	}
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_remove_role(char *role_name){
	generic_list result = get_role_result(REMOVE_ROLE,role_name);
	int value = -1;
	if((*result).role_info!=NULL){
	 	value = (*result).role_info->remove_role;
	}
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_update_role(char *role_name){
	generic_list result = get_role_result(UPDATE_ROLE,role_name);
	int value = -1;
	if((*result).role_info!=NULL){
		value = (*result).role_info->update_role;
	}	
	dealloc(result);	
	return value;
}
