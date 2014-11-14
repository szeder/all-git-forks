
/*	Name		: create_role
	Parameters	: role name and bit array
	Return		: nothing */
void create_role(char *name,char *bit_array);

/*	Name		: update_role
	Parameters	: role name and bit array
	Return		: nothing */
void update_role(char *name,char *bit_array);

/*	Name		: delete_role
	Parameters	: role name
	Return		: nothing */
void delete_role(char *name);

/*	Name		: read_role
	Parameters	: role name
	Return		: nothing */
void read_role(char *name);

/*	Name		: assign_role
	Parameters	: role name and users to add and/or remove
	Return		: nothing */
void assign_role(char *name,char *add,char *rm);

/*	Name		: show_roles
 * 	Parameters	: nothing
 * 	Return		: nothing, print all valid role names */
void show_all();
