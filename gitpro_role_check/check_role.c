#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "check_role.h"
#include "../gitpro_api/gitpro_data_api.h"
#include "../gitpro_api/db_constants.h"

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
	int value = (*result).role_info->read_task;
	dealloc(result);	
	return value;	
}

/* See specification in check_role.h */
int can_assign_task(char *role_name){
	generic_list result = get_role_result(ASSIGN_TASK,role_name);
	int value = (*result).role_info->assign_task;
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_update_task(char *role_name){
	generic_list result = get_role_result(UPDATE_TASK,role_name);
	int value = (*result).role_info->update_task;
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_link_files(char *role_name){
	generic_list result = get_role_result(LINK_FILES,role_name);
	int value = (*result).role_info->link_files;
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_remove_task(char *role_name){
	generic_list result = get_role_result(REMOVE_TASK,role_name);
	int value = (*result).role_info->remove_task;
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_create_task(char *role_name){
	generic_list result = get_role_result(CREATE_TASK,role_name);
	int value = (*result).role_info->create_task;
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_remove_role(char *role_name){
	generic_list result = get_role_result(REMOVE_ROLE,role_name);
	int value = (*result).role_info->remove_role;
	dealloc(result);	
	return value;
}

/* See specification in check_role.h */
int can_update_role(char *role_name){
	generic_list result = get_role_result(UPDATE_ROLE,role_name);
	int value = (*result).role_info->update_role;
	dealloc(result);	
	return value;
}
