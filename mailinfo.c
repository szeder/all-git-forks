#include "cache.h"
#include "mailinfo.h"

void mailinfo_opts_init(struct mailinfo_opts *opts)
{
	memset(opts, 0, sizeof(*opts));
	opts->metainfo_charset = get_commit_output_encoding();
	opts->use_inbody_headers = 1;
	git_config_get_bool("mailinfo.scissors", &opts->use_scissors);
}
