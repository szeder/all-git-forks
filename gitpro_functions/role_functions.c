#include "role_functions.h"
#include "../gitpro_api/gitpro_data_api.h"
#include "../gitpro_api/db_constants.h"

/*******************/
/*PRIVATE FUNCTIONS*/
/*******************/

/*	Name		: separate_bits
	Parameters	: bit array string
	Return		: string with separated bits with following format : 'b1\0b2\0...bn\0'
				 remember to free returned pointer */
char *separate_bits(char *bit_array){
	/* Allocates memory to 1 character plus end string character multiplied by 10 bits */
	char *bits = (char *) malloc((sizeof(char)*2)*10);
	int i = 0;
	int j = 0;
	while(i<10){
		bits[j]=bit_array[i];
		bits[j+1]='\0';
		i++;
		j+=2;
	}
	return bits;
}

/*******************/
/* PUBLIC FUNCTIONS*/
/*******************/

/* See specification on role_functions.h */
void create_role(char *name,char *bit_array){
	char *formatted_name = format_string(name);
	char *upper_name = upper(formatted_name);
	char *insert = NULL;
	char *bits = separate_bits(bit_array);
	char *field_names[] = {ROLE_NAME,CREATE_ROLE,REMOVE_ROLE,UPDATE_ROLE,ASSIGN_ROLE,
						CREATE_TASK,READ_TASK,UPDATE_TASK,REMOVE_TASK,ASSIGN_TASK,LINK_FILES,0};
	char *field_data[] = {upper_name,bits,bits+2,bits+4,bits+6,bits+8,bits+10,
							bits+12,bits+14,bits+16,bits+18,0};
	insert = construct_insert(ROLE_TABLE,field_names,field_data);
	exec_nonquery(insert);
	free(bits);
	free(upper_name);
	free(formatted_name);
	free(insert);
}

/* See specification on role_functions.h */
void update_role(char *name,char *bit_array){
	char *formatted_name = format_string(name);
	char *upper_name = upper(formatted_name);
	char *bits = separate_bits(bit_array);		
	char *update = NULL;
	char *asignations[] = {asig(CREATE_ROLE,bits),asig(REMOVE_ROLE,bits+2),
				asig(UPDATE_ROLE,bits+4),asig(ASSIGN_ROLE,bits+6),asig(CREATE_TASK,bits+8),
				asig(READ_TASK,bits+10),asig(UPDATE_TASK,bits+12),asig(REMOVE_TASK,bits+14),
				asig(ASSIGN_TASK,bits+16),asig(LINK_FILES,bits+18),0};
	update = construct_update(ROLE_TABLE,asignations,
				where(cond(ROLE_NAME,"=",upper_name)));	
	exec_nonquery(update);
	free(bits);
	free(upper_name);
	free(formatted_name);
	free(update);
}

/* See specification on role_functions.h */
void delete_role(char *name){
	char *formatted_name = format_string(name);
	char *upper_name = upper(formatted_name);	
	char *delete = NULL;
	delete = construct_delete(ROLE_TABLE,where(cond(ROLE_NAME,"=",upper_name)));
	exec_nonquery(delete);
	free(upper_name);
	free(formatted_name);
	free(delete);
}

/* See specification on role_functions.h */
void read_role(char *name){
	char *formatted_name = format_string(name);
	char *upper_name = upper(formatted_name);
	char *query = NULL;
	char *fields[] = {ALL,0};
	query = construct_query(fields,from(ROLE_TABLE),
			where(cond(ROLE_NAME,"=",upper_name)),NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	role_list info = (*data).role_info;	
	printf("%s can do following actions:\n",info->role_name);
	if(info->create_role) printf("+ create role\n");
	if(info->remove_role) printf("+ remove role\n");
	if(info->update_role) printf("+ update role\n");
	if(info->assign_role) printf("+ assign role\n");
	if(info->create_task) printf("+ create task\n");
	if(info->read_task) printf("+ read task\n");
	if(info->update_task) printf("+ update task\n");
	if(info->remove_task) printf("+ delete task\n");
	if(info->assign_task) printf("+ assign task\n");
	if(info->link_files) printf("+ link files to task\n");
	info=NULL;
	free(formatted_name);
	free(upper_name);
	free(query);
	dealloc(data);
}

/* See specification on role_functions.h */
void assign_role(char *name,char *add,char *rm){
	char *formatted_role = format_string(name);
	char *upper_role = upper(formatted_role);
	char *temp = strtok(add,",");
	if(add!=NULL){
		printf("Role assigned to following users:\n");
	}
	while(temp!=NULL){
		char *update = NULL;
		char *formatted_user = format_string(temp);
		char *asignations[] = {asig(USER_ROLE,upper_role),0};
		update = construct_update(USER_TABLE,asignations,
					where(cond(USER_NAME,"=",formatted_user)));	
		exec_nonquery(update);
		printf("+ %s\n",temp);
		free(update);
		free(formatted_user);
		temp = strtok(NULL,",");
	}
	temp = strtok(rm,",");
	if(rm!=NULL){
		printf("Role deassigned to following users and set to default (%s):\n",PUBLIC);
	}	
	free(formatted_role);
	free(upper_role);	
	formatted_role = format_string(PUBLIC);
	upper_role = upper(formatted_role);
	while(temp!=NULL){
		char *update = NULL;
		char *formatted_user = format_string(temp);
		char *asignations[] = {asig(USER_ROLE,upper_role),0};
		update = construct_update(USER_TABLE,asignations,
					where(cond(USER_NAME,"=",formatted_user)));	
		exec_nonquery(update);
		printf("- %s\n",temp);
		free(update);
		free(formatted_user);
		temp = strtok(NULL,",");
	}
	free(formatted_role);
	free(upper_role);
}

/* See specification in role_functions.h */
void show_all(){
	char *query = NULL;
	char *fields[] = {ALL,0};
	query = construct_query(fields,from(ROLE_TABLE),
			NULL,NULL,NULL);
	generic_list data = NULL;
	data = exec_query(query);
	if (data==NULL) {
		printf("No existent roles yet\n");
	}else{
		role_list info = (*data).role_info;
		if (info==NULL){
			printf("No existent roles yet\n");
		}else{
			printf("Existent roles:\n");
			while(info!=NULL){
				printf("> %s\n",info->role_name);
				info=info->next;
			}
			dealloc(data);
		}
	}
	free(query);
}
