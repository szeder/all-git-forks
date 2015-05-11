
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
#define EST_TIME 11
#define TIME 12

#define UNAME 14
#define UROLE 15

#define ATID 16
#define AUNAME 17

/* TASK DATA STANDARIZATION */

void std_id(char *id);
void std_name(char *name);
void std_state(char *state);
void std_desc(char *desc);
void std_notes(char *notes);
void std_est_ini(char *est_ini);
void std_est_end(char *est_end);
void std_ini(char *ini);
void std_end(char *end);
void std_priority(char *prior);
void std_type(char *type);
void std_est_time(char *est_time);
void std_time(char *time);

/* USER DATA STANDARIZATION */
/* Generates an id from the uname */
void std_uname(char *uname);
void std_urole(char *urole);

/* ASIG DATA STANDARIZATION */
void std_atid(char *atid);
void std_auname(char *auname);

