#include "cache.h"
#include "rebase-common.h"

void rebase_options_init(struct rebase_options *opts)
{
	oidclr(&opts->onto);
	opts->onto_name = NULL;

	oidclr(&opts->upstream);

	oidclr(&opts->orig_head);
	opts->orig_refname = NULL;

	opts->resolvemsg = NULL;
}

void rebase_options_release(struct rebase_options *opts)
{
	free(opts->onto_name);
	free(opts->orig_refname);
}

void rebase_options_swap(struct rebase_options *dst, struct rebase_options *src)
{
	struct rebase_options tmp = *dst;
	*dst = *src;
	*src = tmp;
}
