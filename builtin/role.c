#include <string.h>
#include "builtin.h"
#include "parse-options.h"
#include "../gitpro_validate/v_codes.h"
#include "../gitpro_role_check/check_role.h"
#include "../gitpro_validate/role_validate.h"
#include "../gitpro_functions/role_functions.h"

#define OUT stdout
#define ERR stderr

static const char * const builtin_role_usage[] =
{
	"role [-c | -r | -u | -d | -a]\n\tSome use examples:\n\t-c -n role_name -p 10_bit_array\n\t-r -n role_name\n\t-u -n role_name -p 10_bit_array\n\t-d -n role_name\n\t-a -n role_name --user --add=\"u1 u2 ... uN\" --rm=\"u1 u2 ... uN\"",
	NULL
};

static int rcreate, rupdate, rdelete, rread, rassign,user,myrole,showall;
static char *role_name = NULL;
static char *perms = NULL;
static char *add = NULL;
static char *rm = NULL;

int cmd_role (int argc, const char **argv, const char *prefix){
	
	static struct option builtin_role_options[] = {
		OPT_GROUP("Role options"),
		OPT_BOOL('r',"read",&rread,N_("check role permissions")),		
		OPT_BOOL('c',"create",&rcreate,N_("creates new role")),
		OPT_BOOL('u',"update",&rupdate,N_("update given role")),		
		OPT_BOOL('d',"delete",&rdelete,N_("remove given role")),
		OPT_BOOL('a',"assign",&rassign,N_("assign role to user")),
		OPT_BOOL(0,"myrole",&myrole,N_("show your role permissions")),
		OPT_BOOL(0,"show-all",&showall,N_("show all existent roles")),
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

	/* Check if username is configured */
	char *uname = get_username();
	if(uname==NULL){
		fputs("Use git config --global user.name your_name to configure name and let them know to administrator\n",OUT);
		return 0;
	}
	
	/* Check if role has been assigned to user */
	char *urole = get_role(uname);
	if(urole==NULL){
		fputs("You haven't been assigned a role.\n",OUT);
		free(uname);
		return 0;
	}

/* START [2.6] Receive data process */
	argc = parse_options(argc, argv, prefix, builtin_role_options, builtin_role_usage, 0);
/* END [2.6] Receive data process */	
	
	if( (rcreate + rread + rdelete + rupdate + rassign + myrole + showall)  > 1){
		fputs(_("Only one option at time\n"),OUT);
		return 0;
	}else{

		if(!rread){
/* START [2.7] Control admin process */
			/* Check if is admin */
			int admin = is_admin(uname);
			if(!admin){
				/* Check role permissions to do selected action if haven't admin privileges */
				if( (rcreate && !can_create_role(urole)) 
					|| (rassign && !can_assign_role(urole))
					|| (rupdate && !can_update_role(urole))
					|| (rdelete && !can_remove_role(urole)) ){
					fputs("You haven't enought permissions to do this action.\n",OUT);
					free(uname);
					free(urole);		
					return 0;
				}
			}
			free(uname);
			free(urole);
/* END [2.7] Control admin process */
		}
		
		if(rcreate){
/* START [2.1.1] Validate data process */
			switch(validate_create_role(role_name,perms,add,rm)){
				case INCORRECT_DATA:
					fputs(_("Incorrect data. Check it all and try again\n"),OUT);
					return 0; 
				case DUPLICATE_ROLE:
					fputs(_("Role name specified already exists\n"),OUT);
					return 0;
			}
/* END [2.1.1] Validate data process */
/* START [2.1.2] Create role option */
			create_role(role_name,perms);
			fputs(_("Role created successfully\n"),OUT);
/* END [2.1.2] Create role option */
		}else if(rread){
/* START [2.4.1] Validate data process */
			switch(validate_read_role(role_name,perms,add,rm)){
				case INCORRECT_DATA:
					fputs(_("Incorrect data. Check it all and try again\n"),OUT);
					return 0;
				case INEXISTENT_ROLE:
					fputs(_("Role you're trying to read doesn't exists\n"),OUT);
					return 0;
			}
/* END [2.4.1] Validate data process */
/* START [2.4.2] Read role option */
			read_role(role_name);
/* END [2.4.2] Read role option */
		}else if(rupdate){
/* START [2.2.1] Validate data process */
			switch(validate_update_role(role_name,perms,add,rm)){
				case INCORRECT_DATA:	
					fputs(_("Incorrect data. Check it all and try again\n"),OUT);
					return 0;
				case INEXISTENT_ROLE:
					fputs(_("Role you're trying to update doesn't exists\n"),OUT);
					return 0;
			}
/* END [2.2.1] Validate data process */
/* START [2.2.2] Update role option */
			update_role(role_name,perms);
			fputs(_("Role updated successfully\n"),OUT);
/* END [2.2.2] Update role option */
		}else if(rdelete){
/* START [2.3.1] Validate data process */
			switch(validate_delete_role(role_name,perms,add,rm)){
				case INCORRECT_DATA:
					fputs(_("Incorrect data. Check it all and try again\n"),OUT);
					return 0;
				case INEXISTENT_ROLE:
					fputs(_("Role you're trying to delete doesn't exists\n"),OUT);
					return 0;
			}
/* END [2.3.1] Validate data process */
/* START [2.3.2] Delete role option */
			delete_role(role_name);
			fputs(_("Role deleted successfully\n"),OUT);
/* END [2.3.2] Delete role option */
		}else if(rassign){
			if(user){
/* START [2.5.1] Validate data process */
				switch(validate_assign_role(role_name,perms,add,rm)){
					case INCORRECT_DATA:
						fputs(_("Incorrect data. Check it all and try again\n"),OUT);
						return 0;
					case INEXISTENT_ROLE:
						fputs(_("Role you're trying to assign doesn't exists\n"),OUT);
						return 0;
					case INEXISTENT_USER:
						fputs(_("User you're trying to assign doesn't exists\n"),OUT);
						return 0;	
				}
/* END [2.5.1] Validate data process */
/* START [2.5.2] Assign role option */
				assign_role(role_name,add,rm);
/* END [2.5.2] Assign role option */
			}else{
				fputs(_("Wrong format, see usage of assign command\n"),OUT);
				return 0;
			}
		}else if(myrole){
/* START [2.4.2] Read role option (facility case to user assigned role) */
			char *uname = get_username();
			char *urole = get_role(uname);
			read_role(urole);
/* END [2.4.2] Read role option (facility case to user assigned role) */
		}else if(showall){
/* START [2.4.2] Read role option (case to check all roles) */	
			show_all();
/* END [2.4.2] Read role option (case to check all roles) */		
		}else{
			/* No action defined */
			fputs(_("No action defined\n"),ERR);
			usage_with_options(builtin_role_usage,builtin_role_options);
			return 0;	
		}
	}
	
	return 1;	
}
