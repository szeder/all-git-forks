#ifndef TWITTER_UTIL_H
#define TWITTER_UTIL_H

/**
 * set an environment variable or die
 */
extern void setenv_overwrite(const char *name, const char *value);

/**
 * set the git useragent with the git version and uname -sr
 */
extern void set_git_useragent(void);

/**
 * sets the max fd soft-limit to the hard-limit
 */
extern void set_max_fd(void);

#endif /* ndef TWITTER_UTIL_H */
