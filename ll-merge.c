/*
 * Low level 3-way in-core file merge.
 *
 * Copyright (c) 2007 Junio C Hamano
 */

#include "cache.h"
#include "attr.h"
#include "xdiff-interface.h"
#include "run-command.h"
#include "ll-merge.h"

struct ll_merge_driver;

typedef int (*ll_merge_fn)(const struct ll_merge_driver *,
			   mmbuffer_t *result,
			   const char *path,
			   mmfile_t *orig, const char *orig_name,
			   mmfile_t *src1, const char *name1,
			   mmfile_t *src2, const char *name2,
			   const struct ll_merge_options *opts,
			   int marker_size);

struct ll_merge_driver {
	const char *name;
	const char *description;
	ll_merge_fn fn;
	const char *recursive;
	struct ll_merge_driver *next;
	char *cmdline;
};

/*
 * Built-in low-levels
 */
static int ll_binary_merge(const struct ll_merge_driver *drv_unused,
			   mmbuffer_t *result,
			   const char *path_unused,
			   mmfile_t *orig, const char *orig_name,
			   mmfile_t *src1, const char *name1,
			   mmfile_t *src2, const char *name2,
			   const struct ll_merge_options *opts,
			   int marker_size)
{
	mmfile_t *stolen;
	assert(opts);

	/*
	 * The tentative merge result is "ours" for the final round,
	 * or common ancestor for an internal merge.  Still return
	 * "conflicted merge" status.
	 */
	stolen = opts->virtual_ancestor ? orig : src1;

	result->ptr = stolen->ptr;
	result->size = stolen->size;
	stolen->ptr = NULL;
	return 1;
}

static int mmbuf_iconv(	const char *fromcode, const char *tocode,
			mmbuffer_t *result,
			mmfile_t   *src,
			const char *pathname,
			const char *filename)
{
	iconv_t iconv_handle = iconv_open(tocode, fromcode);
	size_t inbuf_remain;
	size_t outbuf_remain;
	char *p_in, *p_out;
	int rv_errno = 0;

	if (iconv_handle == (iconv_t)-1) {
		assert(errno != 0);
		rv_errno = errno;
		warning("When merging %s: Failed to initialize iconv from %s to %s: %s",
			pathname, fromcode, tocode, strerror(rv_errno));
		return rv_errno;
	}

	result->size = src->size;
	result->ptr  = malloc(result->size);

	inbuf_remain  = src->size;
	outbuf_remain = result->size;

	p_in  = src->ptr;
	p_out = result->ptr;

	while (inbuf_remain) {
		assert(outbuf_remain + p_out == result->ptr + result->size);

		size_t cvtcount = iconv(iconv_handle,
					&p_in, &inbuf_remain,
					&p_out, &outbuf_remain);
		if (cvtcount != (size_t)-1)
			continue;
		if (errno == E2BIG) {
			// We need to expand the output buffer
			ptrdiff_t cur_offset = p_out - result->ptr;
			size_t expand_sz = result->size;
			if (expand_sz > 16384) // arbitrrary limit
				expand_sz = 16384;

			result->size += expand_sz;
			char *newptr = realloc(result->ptr, result->size);
			if (!newptr)
				goto err_out;

			p_out = cur_offset + newptr;
			result->ptr = newptr;
			outbuf_remain += expand_sz;

			continue;
		}

		// Some other failure
		goto err_out;
	}
	// Shrink the buffer to fit its new contents
	result->size = p_out - result->ptr;
	result->ptr = realloc(result->ptr, result->size);
	assert(result->ptr);

	rv_errno = 0;
	goto out;
err_out:
	rv_errno = errno;
	warning("When merging %s: Failed to convert %s from %s to %s: %s", pathname, filename, fromcode, tocode, strerror(rv_errno));
	free(result->ptr);
	result->ptr = NULL;
	result->size = 0;
out:
	iconv_close(iconv_handle);
	return rv_errno;
}

static int ll_xdl_merge(const struct ll_merge_driver *drv_unused,
			mmbuffer_t *result,
			const char *path,
			mmfile_t *orig, const char *orig_name,
			mmfile_t *src1, const char *name1,
			mmfile_t *src2, const char *name2,
			const struct ll_merge_options *opts,
			int marker_size)
{
	xmparam_t xmp;
	mmbuffer_t orig_cvt = { NULL, 0 };
	mmbuffer_t src1_cvt = { NULL, 0 };
	mmbuffer_t src2_cvt = { NULL, 0 };
	int did_cvt = 0;

	assert(opts);

	if (opts->encoding && strcasecmp(opts->encoding, "UTF-8")) {
		did_cvt = 1;

		if (mmbuf_iconv(opts->encoding, "UTF-8", &orig_cvt, orig, orig_name, path))
			goto binary_fallback;
		if (mmbuf_iconv(opts->encoding, "UTF-8", &src1_cvt, src1, name1, path))
			goto binary_fallback;
		if (mmbuf_iconv(opts->encoding, "UTF-8", &src2_cvt, src2, name2, path))
			goto binary_fallback;

		orig = (mmfile_t *)&orig_cvt;
		src1 = (mmfile_t *)&src1_cvt;
		src2 = (mmfile_t *)&src2_cvt;
	} else {
		if (buffer_is_binary(orig->ptr, orig->size) ||
		    buffer_is_binary(src1->ptr, src1->size) ||
		    buffer_is_binary(src2->ptr, src2->size)) {
			warning("Cannot merge binary files: %s (%s vs. %s)\n",
				path, name1, name2);
			goto binary_fallback;
		}
	}

	memset(&xmp, 0, sizeof(xmp));
	xmp.level = XDL_MERGE_ZEALOUS;
	xmp.favor = opts->variant;
	xmp.xpp.flags = opts->xdl_opts;
	if (git_xmerge_style >= 0)
		xmp.style = git_xmerge_style;
	if (marker_size > 0)
		xmp.marker_size = marker_size;
	xmp.ancestor = orig_name;
	xmp.file1 = name1;
	xmp.file2 = name2;
	int rv = xdl_merge(orig, src1, src2, &xmp, result);

	if (!did_cvt)
		return rv;

	if (rv < 0) {
		free(orig_cvt.ptr);
		free(src1_cvt.ptr);
		free(src2_cvt.ptr);
		return rv;
	}

	mmbuffer_t utf8_result = *result;

	int final_cvt = mmbuf_iconv("UTF-8", opts->encoding, result, (mmfile_t *)&utf8_result, path, "merge result");

	free(utf8_result.ptr);

	if (final_cvt == 0)
		return rv;

	/*
	 * Otherwise, conversion from UTF-8 failed, so fall through to binary merging
	 */
binary_fallback:
	free(orig_cvt.ptr);
	free(src1_cvt.ptr);
	free(src2_cvt.ptr);

	return ll_binary_merge(drv_unused, result,
			       path,
			       orig, orig_name,
			       src1, name1,
			       src2, name2,
			       opts, marker_size);
}

static int ll_union_merge(const struct ll_merge_driver *drv_unused,
			  mmbuffer_t *result,
			  const char *path_unused,
			  mmfile_t *orig, const char *orig_name,
			  mmfile_t *src1, const char *name1,
			  mmfile_t *src2, const char *name2,
			  const struct ll_merge_options *opts,
			  int marker_size)
{
	/* Use union favor */
	struct ll_merge_options o;
	assert(opts);
	o = *opts;
	o.variant = XDL_MERGE_FAVOR_UNION;
	return ll_xdl_merge(drv_unused, result, path_unused,
			    orig, NULL, src1, NULL, src2, NULL,
			    &o, marker_size);
}

#define LL_BINARY_MERGE 0
#define LL_TEXT_MERGE 1
#define LL_UNION_MERGE 2
static struct ll_merge_driver ll_merge_drv[] = {
	{ "binary", "built-in binary merge", ll_binary_merge },
	{ "text", "built-in 3-way text merge", ll_xdl_merge },
	{ "union", "built-in union merge", ll_union_merge },
};

static void create_temp(mmfile_t *src, char *path)
{
	int fd;

	strcpy(path, ".merge_file_XXXXXX");
	fd = xmkstemp(path);
	if (write_in_full(fd, src->ptr, src->size) != src->size)
		die_errno("unable to write temp-file");
	close(fd);
}

/*
 * User defined low-level merge driver support.
 */
static int ll_ext_merge(const struct ll_merge_driver *fn,
			mmbuffer_t *result,
			const char *path,
			mmfile_t *orig, const char *orig_name,
			mmfile_t *src1, const char *name1,
			mmfile_t *src2, const char *name2,
			const struct ll_merge_options *opts,
			int marker_size)
{
	char temp[4][50];
	struct strbuf cmd = STRBUF_INIT;
	struct strbuf_expand_dict_entry dict[5];
	const char *args[] = { NULL, NULL };
	int status, fd, i;
	struct stat st;
	assert(opts);

	dict[0].placeholder = "O"; dict[0].value = temp[0];
	dict[1].placeholder = "A"; dict[1].value = temp[1];
	dict[2].placeholder = "B"; dict[2].value = temp[2];
	dict[3].placeholder = "L"; dict[3].value = temp[3];
	dict[4].placeholder = NULL; dict[4].value = NULL;

	if (fn->cmdline == NULL)
		die("custom merge driver %s lacks command line.", fn->name);

	result->ptr = NULL;
	result->size = 0;
	create_temp(orig, temp[0]);
	create_temp(src1, temp[1]);
	create_temp(src2, temp[2]);
	sprintf(temp[3], "%d", marker_size);

	strbuf_expand(&cmd, fn->cmdline, strbuf_expand_dict_cb, &dict);

	args[0] = cmd.buf;
	status = run_command_v_opt(args, RUN_USING_SHELL);
	fd = open(temp[1], O_RDONLY);
	if (fd < 0)
		goto bad;
	if (fstat(fd, &st))
		goto close_bad;
	result->size = st.st_size;
	result->ptr = xmalloc(result->size + 1);
	if (read_in_full(fd, result->ptr, result->size) != result->size) {
		free(result->ptr);
		result->ptr = NULL;
		result->size = 0;
	}
 close_bad:
	close(fd);
 bad:
	for (i = 0; i < 3; i++)
		unlink_or_warn(temp[i]);
	strbuf_release(&cmd);
	return status;
}

/*
 * merge.default and merge.driver configuration items
 */
static struct ll_merge_driver *ll_user_merge, **ll_user_merge_tail;
static const char *default_ll_merge;

static int read_merge_config(const char *var, const char *value, void *cb)
{
	struct ll_merge_driver *fn;
	const char *ep, *name;
	int namelen;

	if (!strcmp(var, "merge.default")) {
		if (value)
			default_ll_merge = xstrdup(value);
		return 0;
	}

	/*
	 * We are not interested in anything but "merge.<name>.variable";
	 * especially, we do not want to look at variables such as
	 * "merge.summary", "merge.tool", and "merge.verbosity".
	 */
	if (prefixcmp(var, "merge.") || (ep = strrchr(var, '.')) == var + 5)
		return 0;

	/*
	 * Find existing one as we might be processing merge.<name>.var2
	 * after seeing merge.<name>.var1.
	 */
	name = var + 6;
	namelen = ep - name;
	for (fn = ll_user_merge; fn; fn = fn->next)
		if (!strncmp(fn->name, name, namelen) && !fn->name[namelen])
			break;
	if (!fn) {
		fn = xcalloc(1, sizeof(struct ll_merge_driver));
		fn->name = xmemdupz(name, namelen);
		fn->fn = ll_ext_merge;
		*ll_user_merge_tail = fn;
		ll_user_merge_tail = &(fn->next);
	}

	ep++;

	if (!strcmp("name", ep)) {
		if (!value)
			return error("%s: lacks value", var);
		fn->description = xstrdup(value);
		return 0;
	}

	if (!strcmp("driver", ep)) {
		if (!value)
			return error("%s: lacks value", var);
		/*
		 * merge.<name>.driver specifies the command line:
		 *
		 *	command-line
		 *
		 * The command-line will be interpolated with the following
		 * tokens and is given to the shell:
		 *
		 *    %O - temporary file name for the merge base.
		 *    %A - temporary file name for our version.
		 *    %B - temporary file name for the other branches' version.
		 *    %L - conflict marker length
		 *
		 * The external merge driver should write the results in the
		 * file named by %A, and signal that it has done with zero exit
		 * status.
		 */
		fn->cmdline = xstrdup(value);
		return 0;
	}

	if (!strcmp("recursive", ep)) {
		if (!value)
			return error("%s: lacks value", var);
		fn->recursive = xstrdup(value);
		return 0;
	}

	return 0;
}

static void initialize_ll_merge(void)
{
	if (ll_user_merge_tail)
		return;
	ll_user_merge_tail = &ll_user_merge;
	git_config(read_merge_config, NULL);
}

static const struct ll_merge_driver *find_ll_merge_driver(const char *merge_attr)
{
	struct ll_merge_driver *fn;
	const char *name;
	int i;

	initialize_ll_merge();

	if (ATTR_TRUE(merge_attr))
		return &ll_merge_drv[LL_TEXT_MERGE];
	else if (ATTR_FALSE(merge_attr))
		return &ll_merge_drv[LL_BINARY_MERGE];
	else if (ATTR_UNSET(merge_attr)) {
		if (!default_ll_merge)
			return &ll_merge_drv[LL_TEXT_MERGE];
		else
			name = default_ll_merge;
	}
	else
		name = merge_attr;

	for (fn = ll_user_merge; fn; fn = fn->next)
		if (!strcmp(fn->name, name))
			return fn;

	for (i = 0; i < ARRAY_SIZE(ll_merge_drv); i++)
		if (!strcmp(ll_merge_drv[i].name, name))
			return &ll_merge_drv[i];

	/* default to the 3-way */
	return &ll_merge_drv[LL_TEXT_MERGE];
}

static int git_path_check_merge(const char *path, struct git_attr_check check[2])
{
	if (!check[0].attr) {
		check[0].attr = git_attr("merge");
		check[1].attr = git_attr("conflict-marker-size");
		check[2].attr = git_attr("encoding");
	}
	return git_checkattr(path, 3, check);
}

static void normalize_file(mmfile_t *mm, const char *path)
{
	struct strbuf strbuf = STRBUF_INIT;
	if (renormalize_buffer(path, mm->ptr, mm->size, &strbuf)) {
		free(mm->ptr);
		mm->size = strbuf.len;
		mm->ptr = strbuf_detach(&strbuf, NULL);
	}
}

int ll_merge(mmbuffer_t *result_buf,
	     const char *path,
	     mmfile_t *ancestor, const char *ancestor_label,
	     mmfile_t *ours, const char *our_label,
	     mmfile_t *theirs, const char *their_label,
	     const struct ll_merge_options *opts)
{
	static struct git_attr_check check[3];
	static const struct ll_merge_options default_opts;
	struct ll_merge_options file_opts;
	const char *ll_driver_name = NULL;
	int marker_size = DEFAULT_CONFLICT_MARKER_SIZE;
	const struct ll_merge_driver *driver;

	if (!opts)
		file_opts = default_opts;
	else
		file_opts = *opts;

	if (file_opts.renormalize) {
		normalize_file(ancestor, path);
		normalize_file(ours, path);
		normalize_file(theirs, path);
	}
	if (!git_path_check_merge(path, check)) {
		ll_driver_name = check[0].value;
		if (check[1].value) {
			marker_size = atoi(check[1].value);
			if (marker_size <= 0)
				marker_size = DEFAULT_CONFLICT_MARKER_SIZE;
		}
		file_opts.encoding = check[2].value;
	}
	driver = find_ll_merge_driver(ll_driver_name);
	if (file_opts.virtual_ancestor && driver->recursive)
		driver = find_ll_merge_driver(driver->recursive);
	return driver->fn(driver, result_buf, path, ancestor, ancestor_label,
			  ours, our_label, theirs, their_label,
			  &file_opts, marker_size);
}

int ll_merge_marker_size(const char *path)
{
	static struct git_attr_check check;
	int marker_size = DEFAULT_CONFLICT_MARKER_SIZE;

	if (!check.attr)
		check.attr = git_attr("conflict-marker-size");
	if (!git_checkattr(path, 1, &check) && check.value) {
		marker_size = atoi(check.value);
		if (marker_size <= 0)
			marker_size = DEFAULT_CONFLICT_MARKER_SIZE;
	}
	return marker_size;
}
