#include <string.h>
#include "builtin.h"
#include "parse-options.h"
#include "../gitpro_role_check/check_role.h"
#include "../gitpro_api/gitpro_data_api.h"

#define OUT stdout
#define ERR stderr

static const char * const builtin_role_usage[] =
{
	"role [-c | -r | -u | -d | -a]\n\tSome use examples:\n\t-c -n role_name -p 10_bit_array\n\t-r -n role_name\n\t-u -n role_name -p 10_bit_array\n\t-d -n role_name\n\t-a -n role_name -t --add=\"u1 u2 ... uN\" --rm=\"u1 u2 ... uN\"",
	NULL
};

void create_role();
void read_role();
void update_role();
void delete_role();
void assign_role();

static int crole, urole, drole, rrole, arole,user;
static char *role_name = NULL;
static char *perms = NULL;
static char *add = NULL;
static char *rm = NULL;

int cmd_role (int argc, const char **argv, const char *prefix){
	
	static struct option builtin_role_options[] = {
		OPT_GROUP("Role options"),
		OPT_BOOL('r',"read",&rrole,N_("check role permissions")),		
		OPT_BOOL('c',"create",&crole,N_("creates new role")),
		OPT_BOOL('u',"update",&urole,N_("update given role")),		
		OPT_BOOL('d',"delete",&drole,N_("remove given role")),
		OPT_BOOL('a',"assign",&arole,N_("assign role to user")),
		OPT_BOOL(0,"user",&user,N_("indicates that follows user names to add or remove role assignations")),
		OPT_GROUP("Role params"),
		OPT_STRING('n',"name",&role_name,"role name",N_("specifies role name")),
		OPT_STRING('p',"perms",&perms,"permissions",N_("specifies permissions in 10 array bit format")),
		//Format: --add="u1,u2...un"
		OPT_STRING(0,"add",&add,"user1, user2 ... userN",N_("specifies user name to add role assignation")),
		//Format: --rm="u1,u2...un"
		OPT_STRING(0,"rm",&rm,"user1, user2 ... userN",N_("specifies user name to remove role assignation")),
		//Permissions array bit info
		OPT_GROUP("Array bit format example: 10101010101"),
		OPT_GROUP("Array bit meaning"),
		OPT_GROUP("\tbits meaning from left to right\n\t create role\n\t remove role\n\t update role\n\t assign role\n\t create task\n\t read task\n\t update task\n\t delete task\n\t assign task\n\t link files"),
		OPT_END()
	};

/* START [2.6] Receive data process */
	argc = parse_options(argc, argv, prefix, builtin_role_options, builtin_role_usage, 0);
/* END [2.6] Receive data process */	
	
	if(crole + rrole + urole + drole > 1){
		fputs(_("Only one option at time\n"),ERR);
	}else if(crole){
		/* Create role option */
		create_role();
	}else if(rrole){
		/* Read role option */
		read_role();
	}else if(urole){
		/* Update role option */
		update_role();
	}else if(drole){
		/* Delete role option */
		delete_role();
	}else if(arole && user){
		/* Assign role option */
		assign_role();
	}else{
		/* No action defined */
		fputs(_("No action defined\n"),ERR);
		usage_with_options(builtin_role_usage,builtin_role_options);
		return 0;	
	}
	
	return 1;	
}


void create_role(){
	fputs(_("Create role option\n"),OUT);
	if(role_name!=NULL && perms!=NULL){
		printf("%s\n%s\n",role_name,perms);
	}
}

void read_role(){
	fputs(_("Read role option\n"),OUT);
	if(role_name!=NULL && perms==NULL){
		printf("%s\n",role_name);
	}
}

void update_role(){
	fputs(_("Update role option\n"),OUT);
	if(role_name!=NULL && perms!=NULL){
		printf("%s\n%s\n",role_name,perms);
	}
}

void delete_role(){
	fputs(_("Delete role option\n"),OUT);
	if(role_name!=NULL && perms==NULL){
		printf("%s\n",role_name);
	}
}

void assign_role(){
	fputs(_("Assign role option\n"),OUT);
	if(role_name!=NULL && user && (add!=NULL || rm!=NULL)){
		printf("%s\n%s\n%s\n",role_name,add,rm);
	}
}
