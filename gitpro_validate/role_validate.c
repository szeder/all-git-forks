#include "role_validate.h"
#include "../gitpro_api/gitpro_data_api.h"
#include "../gitpro_api/db_constants.h"

int *to_int_bit_array(char *bit_array){
	return NULL;
}


int validate_bit_array(char *bit_array){
	if(bit_array==NULL){
		return INCORRECT_DATA;
	}
	if(strlen(bit_array)!=10){
		return INCORRECT_DATA;
	}
	int i=0;
	for(i=0;i<strlen(bit_array);i++){
		int ascii_char = (int)bit_array[i];		
		if(ascii_char>49 || ascii_char<48){
			return INCORRECT_DATA;
		}
	}
	return DATA_OK;
}

int validate_duplicate_role_name(char *name){
	if(name==NULL){
		return INCORRECT_DATA;
	}
	char *fields[] = {ROLE_NAME,0};
	char *formatted_name = format_string(name);
	char *f_upper_name = upper(formatted_name);
	char *query = NULL;
	query = construct_query(fields,from(ROLE_TABLE),
					where(cond(ROLE_NAME,"=",f_upper_name)),NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	int duplicate = 0;	
	if((*data).role_info!=NULL){
		if(strcmp((*data).role_info->role_name,null_data)!=0){
			duplicate =1;
		}
	}
	free(f_upper_name);
	free(formatted_name);	
	free(query); 
	dealloc(data);	
	if(duplicate){
		return DUPLICATE_ROLE;
	}	
	return DATA_OK;
}

int validate_inexistent_role_name(char *name){
	if(name==NULL) return INCORRECT_DATA;
	char *fields[] = {ROLE_NAME,0};
	char *formatted_name = format_string(name);
	char *f_upper_name = upper(formatted_name);
	char *query = NULL;
	query = construct_query(fields,from(ROLE_TABLE),
					where(cond(ROLE_NAME,"=",f_upper_name)),NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	int ok = 0;
	if((*data).role_info!=NULL){
		if(strcmp((*data).role_info->role_name,null_data)!=0){
			ok=1;
		}
	}
	free(f_upper_name);
	free(formatted_name);
	free(query);
	dealloc(data);
	if(ok) return DATA_OK;
	return INEXISTENT_ROLE;
}

int validate_read_delete_role(char *name,char *bit_array){
	if(bit_array!=NULL){
		return INCORRECT_DATA;
	}
	int name_check = validate_inexistent_role_name(name);
	if(name_check!=DATA_OK) return name_check;
	return DATA_OK;
}

int validate_users(char *users){
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
			return INEXISTENT_USER;
		}
		temp = strtok(NULL,",");
	}
	return DATA_OK;
}

/*******************/
/* PUBLIC FUNCTIONS*/
/*******************/

int validate_create_role(char *name,char *bit_array,char *add,char *rm){
	if(add!=NULL || rm!=NULL){
		return INCORRECT_DATA;
	}
	int bit_array_check = validate_bit_array(bit_array);
	int name_check = validate_duplicate_role_name(name);
	if(bit_array_check!=DATA_OK){
		return bit_array_check;
	}
	if(name_check!=DATA_OK){
		return name_check;
	}
	return DATA_OK;
}

int validate_update_role(char *name,char *bit_array,char *add,char *rm){
	if(add!=NULL || rm!=NULL){
		return INCORRECT_DATA;
	}
	int name_check = validate_inexistent_role_name(name);
	int bit_array_check = validate_bit_array(bit_array);
	if(name_check!=DATA_OK) return name_check;
	if(bit_array_check!=DATA_OK) return bit_array_check;
	return DATA_OK;
}

int validate_delete_role(char *name,char *bit_array,char *add,char *rm){
	if(add!=NULL || rm!=NULL){
		return INCORRECT_DATA;
	}
	return validate_read_delete_role(name,bit_array);
}

int validate_read_role(char *name,char *bit_array,char *add,char *rm){
	if(add!=NULL || rm!=NULL){
		return INCORRECT_DATA;
	}
	return validate_read_delete_role(name,bit_array);
}
 
int validate_assign_role(char *name,char *bit_array,char *add,char *rm){
	if(bit_array!=NULL){
		return INCORRECT_DATA;
	}
	if(add==NULL && rm==NULL){
		return INCORRECT_DATA;
	}
	int name_check = validate_inexistent_role_name(name);
	if(name_check!=DATA_OK) return name_check;
	int users_to_add = validate_users(add);
	if(users_to_add!=DATA_OK) return users_to_add;
	int users_to_rm = validate_users(rm);
	if(users_to_rm!=DATA_OK) return users_to_rm;
	return DATA_OK;
}
