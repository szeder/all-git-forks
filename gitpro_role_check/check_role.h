
/*	Name		: get_role
	Parameters	: user name for which role will be searched (user name had to exists)
	Return		: role name asigned to given user. remember to free returned string with free function in stdlib.h  */
char *get_role(char *username);

/*	Name		: get_username
	Parameters	: nothing
	Return		: username if exists or NULL if user not configured in global config file. remember to free returned string with free function in stdlib.h */
char *get_username();

/*	Name		: can_create_role
	Parameters	: role name to check
	Return		: 1 if that role can create new roles or 0 in other case */
int can_create_role(char *role_name);

/*	Name		: can_assign_role
	Parameters	: role name to check
	Return		: 1 if that role can assign roles to other users or 0 in other case */
int can_assign_role(char *role_name);

/*	Name		: can_read_task
	Parameters	: role name to check
	Return		: 1 if that role can read tasks or 0 in other case */
int can_read_task(char *role_name);

/*	Name		: can_assign_task
	Parameters	: role name to check
	Return		: 1 if that role can assign tasks to users or 0 in other case */
int can_assign_task(char *role_name);

/*	Name		: can_update_task
	Parameters	: role name to check
	Return		: 1 if that role can update existing tasks or 0 in other case */
int can_update_task(char *role_name);

/*	Name		: can_link_files
	Parameters	: role name to check
	Return		: 1 if that role can link files to a task or 0 in other case */
int can_link_files(char *role_name);

/*	Name		: can_remove_task
	Parameters	: role name to check
	Return		: 1 if that role can remove existent tasks or 0 in other case */
int can_remove_task(char *role_name);

/*	Name		: can_create_task
	Parameters	: role name to check
	Return		: 1 if that role can create new tasks or 0 in other case */
int can_create_task(char *role_name);

/*	Name		: can_remove_role
	Parameters	: role name to check
	Return		: 1 if that role can remove existent roles or 0 in other case */
int can_remove_role(char *role_name);

/*	Name		: can_update_role
	Parameters	: role name to check
	Return		: 1 if that role can update existent roles or 0 in other case */
int can_update_role(char *role_name);
