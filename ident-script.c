#include "cache.h"
#include "ident-script.h"

void ident_script_init(struct ident_script *ident)
{
	ident->name = NULL;
	ident->email = NULL;
	ident->date = NULL;
}

void ident_script_release(struct ident_script *ident)
{
	free(ident->name);
	free(ident->email);
	free(ident->date);
	ident_script_init(ident);
}
