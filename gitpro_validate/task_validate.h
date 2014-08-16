
/*	Name		: validate_create_task
	Parameters	: all task data
	Return		: INCORRECT_DATA, DUPLICATE_TASK or DATA_OK */
int validate_create_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm); 
		
/*	Name		: validate_delete_task
	Parameters	: all task data
	Return		: INCORRECT_DATA or DATA_OK */			
int validate_delete_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm);
					
/*	Name		: validate_update_task
	Parameters	: all task data and flag are_filters (1 if params are filters or 0 in other case)
	Return		: INCORRECT_DATA or DATA_OK */			
int validate_update_task(int are_filters,char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm);
					
/*	Name		: validate_read_task
	Parameters	: all task data
	Return		: INCORRECT_DATA or DATA_OK */			
int validate_read_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm);
					
/*	Name		: validate_assign_task
	Parameters	: all task data
	Return		: INEXISTENT_USER, INEXISTENT_TASK, INCORRECT_DATA or DATA_OK */
int validate_assign_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm);
					
/*	Name		: validate_link_task
	Parameters	: all task data
	Return		: INEXISTENT_FILE_FOLDER, INEXISTENT_TASK, INCORRECT_DATA or DATA_OK */
int validate_link_task(char *id,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time,char *add,char *rm);
