#define DATA_OK 0
#define DUPLICATE_ROLE 1
#define INCORRECT_DATA 2
#define INEXISTENT_ROLE 3
#define INEXISTENT_USER 4

//int *to_int_bit_array(char *bit_array);

int validate_create_role(char *name,char *bit_array,char *add,char *rm); 

int validate_update_role(char *name,char *bit_array,char *add,char *rm);

int validate_delete_role(char *name,char *bit_array,char *add,char *rm);

int validate_read_role(char *name,char *bit_array,char *add,char *rm);

int validate_assign_role(char *name,char *bit_array,char *add,char *rm);
