
/*	Name		: validate_create_role
	Parameters	: role name, bit array, users to add and remove
	Return		: INCORRECT_DATA, DUPLICATE_ROLE or DATA_OK */
int validate_create_role(char *name,char *bit_array,char *add,char *rm); 

/*	Name		: validate_update_role
	Parameters	: role name, bit array, users to add and remove
	Return		: INCORRECT_DATA, INEXISTENT_ROLE or DATA_OK */
int validate_update_role(char *name,char *bit_array,char *add,char *rm);

/*	Name		: validate_delete_role
	Parameters	: role name, bit array, users to add and remove
	Return		: INCORRECT_DATA, INEXISTENT_ROLE or DATA_OK */
int validate_delete_role(char *name,char *bit_array,char *add,char *rm);

/*	Name		: validate_read_role
	Parameters	: role name, bit array, users to add and remove
	Return		: INCORRECT_DATA, INEXISTENT_ROLE or DATA_OK */
int validate_read_role(char *name,char *bit_array,char *add,char *rm);

/*	Name		: validate_assign_role
	Parameters	: role name, bit array, users to add and remove
	Return		: INCORRECT_DATA, INEXISTENT_ROLE, INEXISTENT_USER or DATA_OK */
int validate_assign_role(char *name,char *bit_array,char *add,char *rm);
