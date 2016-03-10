#ifndef TWITTER_CONFIG_H
#define TWITTER_CONFIG_H

/* url to post statistics to */
extern const char * stats_url;

/* Boolean set to true to enable stats. */
extern int stats_enabled;

/**
 * Twitter specific config settings.
 */
int git_twitter_config(const char *var, const char *value, void *cb);

#endif /* TWITTER_CONFIG_H */
