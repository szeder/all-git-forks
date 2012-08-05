/*
 * Converts filenames from decomposed unicode into precomposed unicode.
 * Used on MacOS X.
*/


#define PRECOMPOSE_UNICODE_C

#include "cache.h"
#include "utf8.h"
#include "precompose_utf8.h"

typedef char *iconv_ibp;
const static char *repo_encoding = "UTF-8";
const static char *path_encoding = "UTF-8-MAC";


static size_t has_utf8(const char *s, size_t maxlen, size_t *strlen_c)
{
	const uint8_t *utf8p = (const uint8_t*) s;
	size_t strlen_chars = 0;
	size_t ret = 0;

	if ((!utf8p) || (!*utf8p)) {
		return 0;
	}

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
	if (precomposed_unicode != -1)
		return; /* We found it defined in the global config, respect it */
	path[len] = 0;
	strcpy(path + len, auml_nfc);
	output_fd = open(path, O_CREAT|O_EXCL|O_RDWR, 0600);
	if (output_fd >=0) {
		close(output_fd);
		path[len] = 0;
		strcpy(path + len, auml_nfd);
		/* Indicate to the user, that we can configure it to true */
		if (0 == access(path, R_OK))
			git_config_set("core.precomposeunicode", "false");
			/* To be backward compatible, set precomposed_unicode to 0 */
		precomposed_unicode = 0;
		path[len] = 0;
		strcpy(path + len, auml_nfc);
		unlink(path);
	}
}


void precompose_argv(int argc, const char **argv)
{
	int i;
	const char *oldarg;
	char *newarg;
	iconv_t ic_precompose;

	if (precomposed_unicode != 1)
		return;

	/* Avoid iconv_open()/iconv_close() if there is nothing to convert */
	for (i = 0; i < argc; i++) {
		if (has_utf8(argv[i], (size_t)-1, NULL))
			break;
	}
	if (argc <= i)
		return; /* no utf8 found */

	ic_precompose = iconv_open(repo_encoding, path_encoding);
	if (ic_precompose == (iconv_t) -1)
		return;

	for (i = 0; i < argc; i++) {
		size_t namelen;
		oldarg = argv[i];
		if (has_utf8(oldarg, (size_t)-1, &namelen)) {
			newarg = reencode_string_iconv(oldarg, namelen, ic_precompose);
			if (newarg)
				argv[i] = newarg;
		}
	}
	iconv_close(ic_precompose);
}


PREC_DIR *precompose_utf8_opendir(const char *dirname)
{
	PREC_DIR *prec_dir = xmalloc(sizeof(PREC_DIR));
	prec_dir->dirent_nfc = xmalloc(sizeof(dirent_prec_psx));
	prec_dir->dirent_nfc->max_name_len = sizeof(prec_dir->dirent_nfc->d_name);

	prec_dir->dirp = opendir(dirname);
	if (!prec_dir->dirp) {
		free(prec_dir->dirent_nfc);
		free(prec_dir);
		return NULL;
	} else {
		int ret_errno = errno;
		prec_dir->ic_precompose = (iconv_t)-1;
		errno = ret_errno;
	}

	return prec_dir;
}

struct dirent_prec_psx *precompose_utf8_readdir(PREC_DIR *prec_dir)
{
	struct dirent *res;
	res = readdir(prec_dir->dirp);
	if (res) {
		size_t namelenz = strlen(res->d_name) + 1; /* \0 */
		size_t new_maxlen = namelenz;

		int ret_errno = errno;

		if (new_maxlen > prec_dir->dirent_nfc->max_name_len) {
			size_t new_len = sizeof(dirent_prec_psx) + new_maxlen -
				sizeof(prec_dir->dirent_nfc->d_name);

			prec_dir->dirent_nfc = xrealloc(prec_dir->dirent_nfc, new_len);
			prec_dir->dirent_nfc->max_name_len = new_maxlen;
		}

		prec_dir->dirent_nfc->d_ino  = res->d_ino;
		prec_dir->dirent_nfc->d_type = res->d_type;

		if ((precomposed_unicode == 1) && has_utf8(res->d_name, (size_t)-1, NULL)) {
			if (prec_dir->ic_precompose == (iconv_t)-1)
				prec_dir->ic_precompose =
					iconv_open(repo_encoding, path_encoding);
			if (prec_dir->ic_precompose == (iconv_t)-1) {
				die("iconv_open(%s,%s) failed, but needed:\n"
						"    precomposed unicode is not supported.\n"
						"    If you wnat to use decomposed unicode, run\n"
						"    \"git config core.precomposeunicode false\"\n",
						repo_encoding, path_encoding);
			} else {
				iconv_ibp	cp = (iconv_ibp)res->d_name;
				size_t inleft = namelenz;
				char *outpos = &prec_dir->dirent_nfc->d_name[0];
				size_t outsz = prec_dir->dirent_nfc->max_name_len;
				size_t cnt;
				errno = 0;
				cnt = iconv(prec_dir->ic_precompose, &cp, &inleft, &outpos, &outsz);
				if (errno || inleft) {
					/*
					 * iconv() failed and errno could be E2BIG, EILSEQ, EINVAL, EBADF
					 * MacOS X avoids illegal byte sequemces.
					 * If they occur on a mounted drive (e.g. NFS) it is not worth to
					 * die() for that, but rather let the user see the original name
					*/
					namelenz = 0; /* trigger strlcpy */
				}
			}
		}
		else
			namelenz = 0;

		if (!namelenz)
			strlcpy(prec_dir->dirent_nfc->d_name, res->d_name,
							prec_dir->dirent_nfc->max_name_len);

		errno = ret_errno;
		return prec_dir->dirent_nfc;
	}
	return NULL;
}


int precompose_utf8_closedir(PREC_DIR *prec_dir)
{
	int ret_value;
	int ret_errno;
	ret_value = closedir(prec_dir->dirp);
	ret_errno = errno;
	if (prec_dir->ic_precompose != (iconv_t)-1)
		iconv_close(prec_dir->ic_precompose);
	free(prec_dir->dirent_nfc);
	free(prec_dir);
	errno = ret_errno;
	return ret_value;
}
