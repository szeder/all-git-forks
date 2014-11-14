
/*	Name		: create_task
	Parameters	: task data
	Return		: nothing
	Used for	: create a new task with given data */
void create_task(char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time);
	
/*	Name		: filter_task
	Parameters	: all filters
	Return		: filter for task selection (where part with filters for query) 
	Notes		: remember to free returned pointer with free function in stdlib.h */
char *filter_task(char *id,char *name,char *state,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time);
									
/*	Name		: read_task
	Parameters	: task filters
	Return		: nothing
	Used for	: read tasks that matchs with given filter */
void read_task(char *filter);

/*	Name		: update_task
	Parameters	: task filters
	Return		: nothing
	Used for	: update tasks that matchs with fiven filter */
void update_task(char *filter,char *name,char *state,char *desc,char *notes,
					char *est_start,char *est_end,char *start,char *end,char *prior,
					char *type,char *est_time,char *time);
					
/*	Name		: add_assign_task
	Parameters	: task id, users to add
	Return		: nothing
	Used for	: assign task to specified users */
void add_assign_task(char *id,char *add);

/*	Name		: rm_assign_task
	Parameters	: task id, users to rm
	Return		: nothing
	Used for	: deassign task from specified users */
void rm_assign_task(char *id,char *rm);

/*	Name		: rm_task
	Parameters	: filter
	Return		: nothing
	Used for	: remove tasks that match with a given filter and asociations and asignations 
				 implicated with task */
void rm_task(char *filter);

/*	Name		: add_link_files
	Parameters	: task id, files to add
	Return		: nothing
	Used for	: asociate task to specified files */
void add_link_files(char *id,char *add);

/*	Name		: rm_link_files
	Parameters	: task id, files to rm
	Return		: nothing
	Used for	: deasociate task from specified files */
void rm_link_files(char *id,char *rm);

/*	Name		: show_types
	Parameters	: nothing
	Return		: nothing, prints available task types */
void show_types();
