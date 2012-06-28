#define __PRECOMPOSED_UNICODE_C__

#include "../cache.h"
#include "../utf8.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "precomposed_utf8.h"

const static char *repo_encoding = "UTF-8";
const static char *path_encoding = "UTF-8-MAC";


/* Code borrowed from utf8.c */
#if defined(OLD_ICONV) || (defined(__sun__) && !defined(_XPG6))
	typedef const char * iconv_ibp;
#else
	typedef char * iconv_ibp;
#endif

static char *reencode_string_iconv(const char *in, size_t insz, iconv_t conv)
{
	size_t outsz, outalloc;
	char *out, *outpos;
	iconv_ibp cp;

	outsz = insz;
	outalloc = outsz + 1; /* for terminating NUL */
	out = xmalloc(outalloc);
	outpos = out;
	cp = (iconv_ibp)in;

	while (1) {
		size_t cnt = iconv(conv, &cp, &insz, &outpos, &outsz);

		if (cnt == -1) {
			size_t sofar;
			if (errno != E2BIG) {
				free(out);
				return NULL;
			}
			/* insz has remaining number of bytes.
			 * since we started outsz the same as insz,
			 * it is likely that insz is not enough for
			 * converting the rest.
			 */
			sofar = outpos - out;
			outalloc = sofar + insz * 2 + 32;
			out = xrealloc(out, outalloc);
			outpos = out + sofar;
			outsz = outalloc - sofar - 1;
		}
		else {
			*outpos = '\0';
			break;
		}
	}
	return out;
}

static size_t has_utf8(const char *s, size_t maxlen, size_t *strlen_c)
{
	const uint8_t *utf8p = (const uint8_t*) s;
	size_t strlen_chars = 0;
	size_t ret = 0;

	if ((!utf8p) || (!*utf8p))
		return 0;

	while((*utf8p) && maxlen) {
		if (*utf8p & 0x80)
			ret++;
		strlen_chars++;
		utf8p++;
		maxlen--;
	}
	if (strlen_c)
		*strlen_c = strlen_chars;

	return ret;
}


void probe_utf8_pathname_composition(char *path, int len)
{
	const static char *auml_nfc = "\xc3\xa4";
	const static char *auml_nfd = "\x61\xcc\x88";
	int output_fd;
	if (mac_os_precomposed_unicode != -1)
		return; /* We found it defined in the global config, respect it */
	path[len] = 0;
	strcpy(path + len, auml_nfc);
	output_fd = open(path, O_CREAT|O_EXCL|O_RDWR, 0600);
	if (output_fd >=0) {
		close(output_fd);
		path[len] = 0;
		strcpy(path + len, auml_nfd);
		/* Indicate the the user, that we can configure it to true */
		if (0 == access(path, R_OK))
			git_config_set("core.precomposedunicode", "false");
		path[len] = 0;
		strcpy(path + len, auml_nfc);
		unlink(path);
	}
}


void precompose_argv(int argc, const char **argv)
{
	int i = 0;
	const char *oldarg;
	char *newarg;
	iconv_t ic_precompose;

	if (mac_os_precomposed_unicode != 1)
		return;

	ic_precompose = iconv_open(repo_encoding, path_encoding);
	if (ic_precompose == (iconv_t) -1)
		return;

	while (i < argc) {
		size_t namelen;
		oldarg = argv[i];
		if (has_utf8(oldarg, (size_t)-1, &namelen)) {
			newarg = reencode_string_iconv(oldarg, namelen, ic_precompose);
			if (newarg)
				argv[i] = newarg;
		}
		i++;
	}
	iconv_close(ic_precompose);
}


PRECOMPOSED_UTF_DIR * precomposed_utf8_opendir(const char *dirname)
{
	PRECOMPOSED_UTF_DIR *precomposed_utf8_dir;
	precomposed_utf8_dir = xmalloc(sizeof(PRECOMPOSED_UTF_DIR));

	precomposed_utf8_dir->dirp = opendir(dirname);
	if (!precomposed_utf8_dir->dirp) {
		free(precomposed_utf8_dir);
		return NULL;
	}
	precomposed_utf8_dir->ic_precompose = iconv_open(repo_encoding, path_encoding);
	if (precomposed_utf8_dir->ic_precompose == (iconv_t) -1) {
		closedir(precomposed_utf8_dir->dirp);
		free(precomposed_utf8_dir);
		return NULL;
	}

	return precomposed_utf8_dir;
}

struct dirent * precomposed_utf8_readdir(PRECOMPOSED_UTF_DIR *precomposed_utf8_dirp)
{
	struct dirent *res;
	size_t namelen = 0;

	res = readdir(precomposed_utf8_dirp->dirp);
	if (res && (mac_os_precomposed_unicode == 1) && has_utf8(res->d_name, (size_t)-1, &namelen)) {
		int ret_errno = errno;
		size_t outsz = sizeof(precomposed_utf8_dirp->dirent_nfc.d_name) - 1; /* one for \0 */
		char *outpos = precomposed_utf8_dirp->dirent_nfc.d_name;
		iconv_ibp cp;
		size_t cnt;
		size_t insz = namelen;
		cp = (iconv_ibp)res->d_name;

		/* Copy all data except the name */
		memcpy(&precomposed_utf8_dirp->dirent_nfc, res,
		       sizeof(precomposed_utf8_dirp->dirent_nfc)-sizeof(precomposed_utf8_dirp->dirent_nfc.d_name));
		errno = 0;

		cnt = iconv(precomposed_utf8_dirp->ic_precompose, &cp, &insz, &outpos, &outsz);
		if (cnt < sizeof(precomposed_utf8_dirp->dirent_nfc.d_name) -1) {
			*outpos = 0;
			errno = ret_errno;
			return &precomposed_utf8_dirp->dirent_nfc;
		}
		errno = ret_errno;
	}
	return res;
}


int precomposed_utf8_closedir(PRECOMPOSED_UTF_DIR *precomposed_utf8_dirp)
{
	int ret_value;
	int ret_errno;
	ret_value = closedir(precomposed_utf8_dirp->dirp);
	ret_errno = errno;
	if (precomposed_utf8_dirp->ic_precompose != (iconv_t)-1)
		iconv_close(precomposed_utf8_dirp->ic_precompose);
	free(precomposed_utf8_dirp);
	errno = ret_errno;
	return ret_value;
}
