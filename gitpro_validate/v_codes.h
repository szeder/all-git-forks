#define DATA_OK 0			//Constant to represent all data is valid
#define DUPLICATE_ROLE 1	//Constant to represent role name already exists
#define INCORRECT_DATA 2	//Constant to represent some data isn't valid
#define INEXISTENT_ROLE 3	//Constant to represent role name does not exist
#define INEXISTENT_USER 4	//Constant to represent some user name does not exist
#define DUPLICATE_TASK 5 	//Constant to represent task already exists
#define INEXISTENT_TASK 6	//Constant to represent task id does not exist
#define INEXISTENT_FILE_FOLDER 7 //Constant to represent file or folder that not exist

#define SWITCH_EMPTY 8 //Constant to represent there's no an active task by switch command
#define SWITCH_SAME 9 //Constant to represent that action is a switch on a previous switched task
#define SWITCH_OTHER 10 //Constant to represent that action is a switch on other task distinct than previous
