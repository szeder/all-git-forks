#ifndef __PRECOMPOSED_UNICODE_H__
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <iconv.h>


typedef struct {
	iconv_t ic_precompose;
	DIR *dirp;
	struct dirent dirent_nfc;
} PRECOMPOSED_UTF_DIR;

char *precompose_str(const char *in, iconv_t ic_precompose);
void precompose_argv(int argc, const char **argv);
void probe_utf8_pathname_composition(char *, int);

PRECOMPOSED_UTF_DIR *precomposed_utf8_opendir(const char *dirname);
struct dirent *precomposed_utf8_readdir(PRECOMPOSED_UTF_DIR *dirp);
int precomposed_utf8_closedir(PRECOMPOSED_UTF_DIR *dirp);

#ifndef __PRECOMPOSED_UNICODE_C__
#define opendir(n) precomposed_utf8_opendir(n)
#define readdir(d) precomposed_utf8_readdir(d)
#define closedir(d) precomposed_utf8_closedir(d)
#define DIR PRECOMPOSED_UTF_DIR
#endif /* __PRECOMPOSED_UNICODE_C__ */

#define  __PRECOMPOSED_UNICODE_H__
#endif /* __PRECOMPOSED_UNICODE_H__ */
