#include <stdlib.h>
#include <string.h>

/* Data struct of role info representation in database */
struct role_node {
	char *role_name; //Unique name of role
	int create_role; //1 if can create new roles or 0 in other case
	int assign_role; //1 if can assign roles or 0 in other case
	int read_task; //1 if can read tasks or 0 in other case
	int assign_task; //1 if can assign tasks to other users or 0 in other case
	int update_task; //1 if can update existing task or 0 in other case
	int link_files; //1 if can link files to existent task or 0 in other case
	int remove_task; //1 if can remove an existent task or 0 in other case
	int create_task; //1 if can create new tasks or 0 in other case
	int remove_role; //1 if can remove existent roles or 0 in other case
	int update_role; //1 if can update existent roles or 0 in other case
	struct role_node *next; //pointer to next role node
};

/* Data struct of task info representation in database */
struct task_node {
	int task_id; //Unique identifier of task
	char *task_name; //Name of task
	char *state; //State of task
	char *description; //Description of what to do in this task
	char *notes; //Notes and observations 
	char *type; //Type of task (for example, DEVELOPMENT)
	char *priority; //Priority over other tasks (for example, HIGH)
	int real_time_min; //Real time until task has finished
	int est_time_min; //Estimated time to do task. Usually set when created
	struct task_node *next; //pointer to next task node
};

/* Data struct of user info representation in database */
struct user_node {
	char *user_name; //Unique user name
	char *user_role; //Role asigned to user
	struct user_node *next; //pointer to next user node
};	

/* Data struct of file info representation in database */
struct file_node {
	char *file_name; //File name
	char *file_path; //Unique file path
	struct file_node *next; //pointer to next file node
};

/* Data struct of asociations info representation in database */
struct asoc_node {
	char *file_path; //Path of file asociated to that task
	int task_id; //Task id
	struct asoc_node *next; //pointer to next asociation node
};

/* Data struct of asignations info representation in database */
struct asig_node {
	char *user_name; //User name asigned to a task
	int task_id; //Task id
	struct asig_node *next; //pointer to next asignation node
};

/* Specific lists type definitions */
typedef struct role_node *role_list;
typedef struct task_node *task_list;
typedef struct user_node *user_list;
typedef struct file_node *file_list;
typedef struct asoc_node *asoc_list;
typedef struct asig_node *asig_list;

/* Data struct returned in query execution with one of each lists */
struct generic_node {
	role_list role_info;
	task_list task_info;
	user_list user_info;
	file_list file_info;
	asoc_list asoc_info;
	asig_list asig_info;
};

/* Generic list type definition */
typedef struct generic_node *generic_list;

/* 	Name		: insert_role
	Parameters	: role_list and all role data to be inserted
	Return		: nothing
	Used for	: fill role_list that will be in generic list */
void insert_role(role_list *list,char *name,int create_role,int assign_role,
				int read_task,int assign_task,int update_task,int link_files,
				int remove_task,int create_task,int remove_role,int update_role);

/* 	Name		: insert_task
	Parameters	: task_list and all task data to be inserted
	Return		: nothing
	Used for	: fill task_list that will be in generic list */
void insert_task(task_list *list,int id,char *name,char *state,char *description,
				char *notes,char *type,char *priority,int real_time,int est_time);

/* 	Name		: insert_user
	Parameters	: user_list and all user data to be inserted
	Return		: nothing
	Used for	: fill user_list that will be in generic list */
void insert_user(user_list *list,char *name,char *urole);

/* 	Name		: insert_file
	Parameters	: file_list and all file data to be inserted
	Return		: nothing
	Used for	: fill file_list that will be in generic list */
void insert_file(file_list *list,char *name,char *path);

/* 	Name		: insert_asoc
	Parameters	: asoc_list and all asociation data to be inserted
	Return		: nothing
	Used for	: fill asoc_list that will be in generic list */
void insert_asoc(asoc_list *list,char *path,int task_id);

/* 	Name		: insert_asig
	Parameters	: asig_list and all asignation data to be inserted
	Return		: nothing
	Used for	: fill asig_list that will be in generic list */
void insert_asig(asig_list *list,char *user_name,int task_id);

/* 	Name		: dealloc_roles
	Parameters	: role_list to be freed
	Return		: nothing
	Used for	: free all allocated memory that store role nodes */
void dealloc_roles(role_list list);

/* 	Name		: dealloc_tasks
	Parameters	: task_list to be freed
	Return		: nothing
	Used for	: free all allocated memory that store task nodes */
void dealloc_tasks(task_list list);

/* 	Name		: dealloc_users
	Parameters	: user_list to be freed
	Return		: nothing
	Used for	: free all allocated memory that store user nodes */
void dealloc_users(user_list list);

/* 	Name		: dealloc_files
	Parameters	: file_list to be freed
	Return		: nothing
	Used for	: free all allocated memory that store file nodes */
void dealloc_files(file_list list);

/* 	Name		: dealloc_asocs
	Parameters	: asoc_list to be freed
	Return		: nothing
	Used for	: free all allocated memory that store asociation nodes */
void dealloc_asocs(asoc_list list);

/* 	Name		: dealloc_asigs
	Parameters	: asig_list to be freed
	Return		: nothing
	Used for	: free all allocated memory that store asignation nodes */
void dealloc_asigs(asig_list list);
