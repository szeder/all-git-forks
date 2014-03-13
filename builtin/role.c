#include <string.h>
#include "builtin.h"
#include "parse-options.h"

#define OUT stdout
#define ERR stderr

static const char * const builtin_role_usage[] =
{
	"role [-c | -r | -u | -d]",
	NULL
};

void create_role();
void read_role();
void update_role();
void delete_role();

static int crole, rrole, urole, drole;

int cmd_role (int argc, const char **argv, const char *prefix){

	static struct option builtin_role_options[] = {
		OPT_GROUP("Role options"),
		OPT_BOOL('c',0,&crole,N_("creates new role")),
		OPT_BOOL('r',0,&rrole,N_("show roles")),
		OPT_BOOL('u',0,&urole,N_("update given role")),		
		OPT_BOOL('d',0,&drole,N_("remove given role")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, builtin_role_options, builtin_role_usage, 0);

	if(crole + rrole + urole + drole > 1){
		fputs(_("Only one option at time\n"),ERR);
	}else if(crole){
		create_role();
	}else if(rrole){
		read_role();
	}else if(urole){
		update_role();
	}else if(drole){
		delete_role();
	}else{
		fputs(_("No action defined\n"),ERR);
		usage_with_options(builtin_role_usage,builtin_role_options);
		return 0;	
	}
	return 1;	
}


void create_role(){
	fputs(_("Create role option\n"),OUT);
}

void read_role(){
	fputs(_("Read role option\n"),OUT);
}

void update_role(){
	fputs(_("Update role option\n"),OUT);
}

void delete_role(){
	fputs(_("Delete role option\n"),OUT);
}
