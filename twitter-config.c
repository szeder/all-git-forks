#include "cache.h"
#include "twitter-config.h"

/* config globals */
const char * stats_url = "http://build.twitter.biz/gitstats/2.0/publish";
int stats_enabled = 0;

int git_twitter_config(const char *var, const char *value, void *cb)
{
	if (!strcmp(var, "twitter.statsenabled")) {
		trace_printf("twitter-config: settings stats enabled\n");
		stats_enabled = git_config_bool(var, value);
		return 0;
	}

	if (!strcmp(var, "twitter.statsurl")) {
		return git_config_string(&stats_url, var, value);
	}
	return 0;
}
