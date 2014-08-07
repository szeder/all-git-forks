#define DATA_OK 0			//Constant to represent all data is valid
#define DUPLICATE_ROLE 1	//Constant to represent role name already exists
#define INCORRECT_DATA 2	//Constant to represent some data isn't valid
#define INEXISTENT_ROLE 3	//Constant to represent role name does not exist
#define INEXISTENT_USER 4	//Constant to represent some user name does not exist

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
