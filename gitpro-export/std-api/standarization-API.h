/* This constants is only to make easier analyzer implementation */
#define TASK -3
#define USER -2
#define ASIG -1

#define NONE -10

#define ID 0
#define NAME 1
#define STATE 2
#define DESC 3
#define NOTES 4
#define EST_INI 5
#define EST_END 6
#define INI 7
#define END 8
#define PRIOR 9
#define TYPE 10
#define TIME 11
#define EST_TIME 12

#define UID 13
#define UNAME 14
#define UROLE 15

#define AUNAME 16
#define ATID 17

/***********************************************/
/*****	       PUBLIC FUNCTIONS           ******/
/***********************************************/

void set_id(char *id);
void set_name(char *name);
void set_state(char *state);
void set_desc(char *desc);
void set_notes(char *notes);
void set_est_ini(char *est_ini);
void set_est_end(char *est_end);
void set_ini(char *ini);
void set_end(char *end);
void set_priority(char *prior);
void set_type(char *type);
void set_est_time(char *est_time);
void set_time(char *time);

void set_uname(char *uname);
void set_urole(char *urole);

void set_auname(char *auname);
void set_atid(char *atid);

/* Call this functions when all parameters you want standarize is
 * set with its set_xxx function */
void std_task();
void std_user();
void std_asig();
