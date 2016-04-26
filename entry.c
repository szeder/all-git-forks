#include "cache.h"
#include "blob.h"
#include "dir.h"
#include "streaming.h"
#include "entry.h"
#include "run-command.h"
#include "argv-array.h"
#include "pkt-line.h"
#include "progress.h"

struct checkout_item {
	struct cache_entry *ce;
	struct checkout_item *next;
};

struct checkout_worker {
	struct worker_process wp;

	struct checkout_item *to_complete;
	struct checkout_item *to_send;
	int nr_to_send;
	struct strbuf sending;
	int nr_sending, bytes_sent;
};

struct parallel_checkout {
	struct process_pool pp;
	struct checkout state;
	struct checkout_item *items;
	int nr_items, alloc_items;
	int nr_workers;
	int errs;
	struct progress *progress;
	int progress_count;
};

static struct parallel_checkout *parallel_checkout;

static int queue_checkout(struct parallel_checkout *,
			  const struct checkout *,
			  struct cache_entry *);

static void create_directories(const char *path, int path_len,
			       const struct checkout *state)
{
	char *buf = xmallocz(path_len);
	int len = 0;

	while (len < path_len) {
		do {
			buf[len] = path[len];
			len++;
		} while (len < path_len && path[len] != '/');
		if (len >= path_len)
			break;
		buf[len] = 0;

		/*
		 * For 'checkout-index --prefix=<dir>', <dir> is
		 * allowed to be a symlink to an existing directory,
		 * and we set 'state->base_dir_len' below, such that
		 * we test the path components of the prefix with the
		 * stat() function instead of the lstat() function.
		 */
		if (has_dirs_only_path(buf, len, state->base_dir_len))
			continue; /* ok, it is already a directory. */

		/*
		 * If this mkdir() would fail, it could be that there
		 * is already a symlink or something else exists
		 * there, therefore we then try to unlink it and try
		 * one more time to create the directory.
		 */
		if (mkdir(buf, 0777)) {
			if (errno == EEXIST && state->force &&
			    !unlink_or_warn(buf) && !mkdir(buf, 0777))
				continue;
			die_errno("cannot create directory at '%s'", buf);
		}
	}
	free(buf);
}

static void remove_subtree(struct strbuf *path)
{
	DIR *dir = opendir(path->buf);
	struct dirent *de;
	int origlen = path->len;

	if (!dir)
		die_errno("cannot opendir '%s'", path->buf);
	while ((de = readdir(dir)) != NULL) {
		struct stat st;

		if (is_dot_or_dotdot(de->d_name))
			continue;

		strbuf_addch(path, '/');
		strbuf_addstr(path, de->d_name);
		if (lstat(path->buf, &st))
			die_errno("cannot lstat '%s'", path->buf);
		if (S_ISDIR(st.st_mode))
			remove_subtree(path);
		else if (unlink(path->buf))
			die_errno("cannot unlink '%s'", path->buf);
		strbuf_setlen(path, origlen);
	}
	closedir(dir);
	if (rmdir(path->buf))
		die_errno("cannot rmdir '%s'", path->buf);
}

static int create_file(const char *path, unsigned int mode)
{
	mode = (mode & 0100) ? 0777 : 0666;
	return open(path, O_WRONLY | O_CREAT | O_EXCL, mode);
}

static void *read_blob_entry(const struct cache_entry *ce, unsigned long *size)
{
	enum object_type type;
	void *new = read_sha1_file(ce->sha1, &type, size);

	if (new) {
		if (type == OBJ_BLOB)
			return new;
		free(new);
	}
	return NULL;
}

static int open_output_fd(char *path, const struct cache_entry *ce, int to_tempfile)
{
	int symlink = (ce->ce_mode & S_IFMT) != S_IFREG;
	if (to_tempfile) {
		xsnprintf(path, TEMPORARY_FILENAME_LENGTH, "%s",
			  symlink ? ".merge_link_XXXXXX" : ".merge_file_XXXXXX");
		return mkstemp(path);
	} else {
		return create_file(path, !symlink ? ce->ce_mode : 0666);
	}
}

static int fstat_output(int fd, const struct checkout *state, struct stat *st)
{
	/* use fstat() only when path == ce->name */
	if (fstat_is_reliable() &&
	    state->refresh_cache && !state->base_dir_len) {
		fstat(fd, st);
		return 1;
	}
	return 0;
}

static int streaming_write_entry(const struct cache_entry *ce, char *path,
				 struct stream_filter *filter,
				 const struct checkout *state, int to_tempfile,
				 int *fstat_done, struct stat *statbuf)
{
	int result = 0;
	int fd;

	fd = open_output_fd(path, ce, to_tempfile);
	if (fd < 0)
		return -1;

	result |= stream_blob_to_fd(fd, ce->sha1, filter, 1);
	*fstat_done = fstat_output(fd, state, statbuf);
	result |= close(fd);

	if (result)
		unlink(path);
	return result;
}

static int write_entry(struct cache_entry *ce,
		       char *path, const struct checkout *state, int to_tempfile)
{
	unsigned int ce_mode_s_ifmt = ce->ce_mode & S_IFMT;
	int fd, ret, fstat_done = 0;
	char *new;
	struct strbuf buf = STRBUF_INIT;
	unsigned long size;
	size_t wrote, newsize = 0;
	struct stat st;

	if (ce_mode_s_ifmt == S_IFREG) {
		struct stream_filter *filter = get_stream_filter(ce->name, ce->sha1);
		if (filter &&
		    !streaming_write_entry(ce, path, filter,
					   state, to_tempfile,
					   &fstat_done, &st))
			goto finish;
	}

	switch (ce_mode_s_ifmt) {
	case S_IFREG:
	case S_IFLNK:
		new = read_blob_entry(ce, &size);
		if (!new)
			return error("unable to read sha1 file of %s (%s)",
				path, sha1_to_hex(ce->sha1));

		if (ce_mode_s_ifmt == S_IFLNK && has_symlinks && !to_tempfile) {
			ret = symlink(new, path);
			free(new);
			if (ret)
				return error("unable to create symlink %s (%s)",
					     path, strerror(errno));
			break;
		}

		/*
		 * Convert from git internal format to working tree format
		 */
		if (ce_mode_s_ifmt == S_IFREG &&
		    convert_to_working_tree(ce->name, new, size, &buf)) {
			free(new);
			new = strbuf_detach(&buf, &newsize);
			size = newsize;
		}

		fd = open_output_fd(path, ce, to_tempfile);
		if (fd < 0) {
			free(new);
			return error("unable to create file %s (%s)",
				path, strerror(errno));
		}

		wrote = write_in_full(fd, new, size);
		if (!to_tempfile)
			fstat_done = fstat_output(fd, state, &st);
		close(fd);
		free(new);
		if (wrote != size)
			return error("unable to write file %s", path);
		break;
	case S_IFGITLINK:
		if (to_tempfile)
			return error("cannot create temporary submodule %s", path);
		if (mkdir(path, 0777) < 0)
			return error("cannot create submodule directory %s", path);
		break;
	default:
		return error("unknown file mode for %s in index", path);
	}

finish:
	if (state->refresh_cache) {
		assert(state->istate);
		if (!fstat_done)
			lstat(ce->name, &st);
		fill_stat_cache_info(ce, &st);
		ce->ce_flags |= CE_UPDATE_IN_BASE;
		state->istate->cache_changed |= CE_ENTRY_CHANGED;
	}
	return 0;
}

/*
 * This is like 'lstat()', except it refuses to follow symlinks
 * in the path, after skipping "skiplen".
 */
static int check_path(const char *path, int len, struct stat *st, int skiplen)
{
	const char *slash = path + len;

	while (path < slash && *slash != '/')
		slash--;
	if (!has_dirs_only_path(path, slash - path, skiplen)) {
		errno = ENOENT;
		return -1;
	}
	return lstat(path, st);
}

/*
 * Write the contents from ce out to the working tree.
 *
 * When topath[] is not NULL, instead of writing to the working tree
 * file named by ce, a temporary file is created by this function and
 * its name is returned in topath[], which must be able to hold at
 * least TEMPORARY_FILENAME_LENGTH bytes long.
 */
int checkout_entry(struct cache_entry *ce,
		   const struct checkout *state, char *topath)
{
	static struct strbuf path = STRBUF_INIT;
	struct stat st;

	if (topath)
		return write_entry(ce, topath, state, 1);

	strbuf_reset(&path);
	strbuf_add(&path, state->base_dir, state->base_dir_len);
	strbuf_add(&path, ce->name, ce_namelen(ce));

	if (!check_path(path.buf, path.len, &st, state->base_dir_len)) {
		unsigned changed = ce_match_stat(ce, &st, CE_MATCH_IGNORE_VALID|CE_MATCH_IGNORE_SKIP_WORKTREE);
		if (!changed)
			return 0;
		if (!state->force) {
			if (!state->quiet)
				fprintf(stderr,
					"%s already exists, no checkout\n",
					path.buf);
			return -1;
		}

		/*
		 * We unlink the old file, to get the new one with the
		 * right permissions (including umask, which is nasty
		 * to emulate by hand - much easier to let the system
		 * just do the right thing)
		 */
		if (S_ISDIR(st.st_mode)) {
			/* If it is a gitlink, leave it alone! */
			if (S_ISGITLINK(ce->ce_mode))
				return 0;
			if (!state->force)
				return error("%s is a directory", path.buf);
			remove_subtree(&path);
		} else if (unlink(path.buf))
			return error("unable to unlink old '%s' (%s)",
				     path.buf, strerror(errno));
	} else if (state->not_new)
		return 0;

	create_directories(path.buf, path.len, state);

	if (!queue_checkout(parallel_checkout, state, ce))
		/*
		 * write_entry() will be done by parallel_checkout_worker() in
		 * a separate process
		 */
		return 0;

	return write_entry(ce, path.buf, state, 0);
}

int start_parallel_checkout(const struct checkout *state)
{
	if (parallel_checkout)
		die("BUG: parallel checkout already initiated");
	parallel_checkout = xmalloc(sizeof(*parallel_checkout));
	memset(parallel_checkout, 0, sizeof(*parallel_checkout));
	memcpy(&parallel_checkout->state, state, sizeof(*state));

	return 0;
}

static int queue_checkout(struct parallel_checkout *pc,
			  const struct checkout *state,
			  struct cache_entry *ce)
{
	struct checkout_item *ci;

	if (!pc ||
	    !S_ISREG(ce->ce_mode) ||
	    memcmp(&pc->state, state, sizeof(*state)))
		return -1;

	ALLOC_GROW(pc->items, pc->nr_items + 1, pc->alloc_items);
	ci = pc->items + pc->nr_items++;
	ci->ce = ce;
	return 0;
}

int nr_checkouts_queued(void)
{
	return parallel_checkout ? parallel_checkout->nr_items : 0;
}

static int item_cmp(const void *a_, const void *b_)
{
	const struct checkout_item *a = a_;
	const struct checkout_item *b = b_;
	return strcmp(a->ce->name, b->ce->name);
}

static int send_to_worker(struct worker_process *wp, int revents, void *data);
static int receive_from_worker(struct worker_process *wp, int revents, void *data);
static int setup_workers(struct parallel_checkout *pc)
{
	int i, from, nr_per_worker;

	qsort(pc->items, pc->nr_items, sizeof(*pc->items), item_cmp);
	nr_per_worker = pc->nr_items / pc->nr_workers + 1;
	from = 0;

	for (i = 0; i < pc->nr_workers; i++) {
		struct checkout_worker *worker;
		struct child_process *cp;
		struct checkout_item *item;
		int to;

		worker = xmalloc(sizeof(*worker));
		memset(worker, 0, sizeof(*worker));
		strbuf_init(&worker->sending, 0);
		cp = &worker->wp.cp;

		to = from + nr_per_worker;
		if (i == pc->nr_workers - 1)
			to = pc->nr_items;
		item = NULL;
		while (from < to) {
			pc->items[from].next = item;
			item = pc->items + from;
			from++;
			worker->nr_to_send++;
		}
		worker->to_send = item;
		worker->to_complete = item;

		cp->git_cmd = 1;
		cp->in = -1;
		cp->out = -1;
		argv_array_push(&cp->args, "checkout-index");
		argv_array_push(&cp->args, "--worker");
		if (start_command(cp))
			die(_("failed to run checkout worker"));

		/*
		 * state.quiet and state.not_new are not used by
		 * write_entry(). state.refresh_cache() is handled in
		 * main process. No need to send them.
		 */
		if (pc->state.force)
			packet_write(cp->in, "option force");

		poll_worker_output(&pc->pp, &worker->wp,
				   receive_from_worker, pc);
		poll_worker_input(&pc->pp, &worker->wp,
				  send_to_worker, pc);
	}
	return 0;
}

static void progress_one(struct parallel_checkout *pc)
{
	if (!pc->progress)
		return;
	display_progress(pc->progress, ++pc->progress_count);
}

static void close_and_clear(int *fd)
{
	if (*fd != -1) {
		close(*fd);
		*fd = -1;
	}
}

static int finish_worker(struct parallel_checkout *pc,
			 struct checkout_worker *worker)
{
	remove_process_from_pool(&pc->pp, &worker->wp);
	close_and_clear(&worker->wp.cp.in);
	close_and_clear(&worker->wp.cp.out);
	if (finish_command(&worker->wp.cp))
		die(_("checkout worker fails"));
	free(worker);
	pc->nr_workers--;
	return 0;
}

static int steal_from_other_workers(struct parallel_checkout *pc,
				    struct checkout_worker *worker)
{
	struct checkout_worker *laziest, *wp;
	struct checkout_item *item;
	int i, nr1, nr2;

	if (worker->to_send)
		return 1;

	assert(worker->nr_to_send == 0);
	laziest = wp = (struct checkout_worker *)pc->pp.worker;
	while (wp) {
		if (wp->nr_to_send > laziest->nr_to_send)
			laziest = wp;
		wp = (struct checkout_worker *)wp->wp.next;
	}
	if (laziest->nr_to_send - laziest->nr_sending < 4)
		return 0;

	nr1 = (laziest->nr_to_send - laziest->nr_sending) / 2;
	nr1 += laziest->nr_sending;
	nr2 = laziest->nr_to_send - nr1;
	item = laziest->to_send;
	for (i = 0; i < nr1 - 1; i++)
		item = item->next;
	worker->to_send = item->next;
	worker->nr_to_send = nr2;
	if (worker->to_complete) {
		struct checkout_item *tail = worker->to_complete;
		while (tail->next)
			tail = tail->next;
		tail->next = worker->to_send;
	} else
		worker->to_complete = worker->to_send;
	item->next = NULL;
	laziest->nr_to_send = nr1;

	return worker->to_send != NULL;
}

static int send_to_worker(struct worker_process *wp, int revents, void *data)
{
	struct checkout_worker *worker = (struct checkout_worker *)wp;
	struct parallel_checkout *pc = data;
	int fd = worker->wp.cp.in, i;
	struct checkout_item *to_send;

	if (revents & (POLLHUP | POLLERR | POLLNVAL)) {
		pc->errs++;
		return 0;
	}

	if (worker->bytes_sent < worker->sending.len) {
		const char *p = worker->sending.buf + worker->bytes_sent;
		int sz = worker->sending.len - worker->bytes_sent;
		int ret = xwrite(fd, p, sz);
		if (ret <= 0) {
			error("failed to write: %s", strerror(errno));
			pc->errs++;
			return 0;
		}
		worker->bytes_sent += ret;
	}

	if (worker->bytes_sent < worker->sending.len)
		return POLLOUT;

	while (worker->nr_sending) {
		worker->to_send = worker->to_send->next;
		worker->nr_to_send--;
		worker->nr_sending--;
	}
	strbuf_reset(&worker->sending);
	worker->bytes_sent = 0;

	if (!worker->to_send && !steal_from_other_workers(pc, worker)) {
		packet_flush(fd);
		close_and_clear(&worker->wp.cp.in);
		return 0;
	}

	to_send = worker->to_send;
	for (i = 0; to_send && i < 16; i++) {
		packet_buf_write(&worker->sending, "%s %s",
				 sha1_to_hex(to_send->ce->sha1),
				 to_send->ce->name);
		worker->nr_sending++;
		to_send = to_send->next;
	}
	packet_buf_write(&worker->sending, "report");

	return POLLOUT;
}

static void report_pending_ok(int *pending_ok)
{
	if (*pending_ok) {
		packet_write(1, "OK %d", *pending_ok);
		*pending_ok = 0;
	}
}

/*
 * Protocol between main checkout process and the worker is quite
 * simple. All messages are packaged in pkt-line format. PKT-FLUSH
 * marks the end of input (from both sides). The main process issues
 * remote write_entry() with a line
 *
 *    <SHA-1> <SPC> <PATH>
 *
 * The worker reponds with "OK <n>", which is the number of
 * write_entry() has been completed since the last response. On error,
 * the worker can send "ERR" or "RET" line back and terminate itself.
 *
 * The main process can send "option" lines at the beginning to
 * initialize parameters in struct checkout.
 */
int parallel_checkout_worker(void)
{
	struct checkout state;
	struct cache_entry *ce = NULL;
	int pending_ok = 0;

	memset(&state, 0, sizeof(state));
	for (;;) {
		int len, ret;
		unsigned char sha1[20];
		const char *line = packet_read_line(0, &len);

		if (!line) {
			report_pending_ok(&pending_ok);
			packet_flush(1);
			return 0;
		}

		if (skip_prefix(line, "option ", &line)) {
			if (!strcmp(line, "force"))
				state.force = 1;
			else
				die(_("checkout worker: unrecognized option %s"), line);
			continue;
		}
		if (!strcmp(line, "report")) {
			report_pending_ok(&pending_ok);
			continue;
		}

		if (len < 41)
			die(_("checkout worker: invalid format %s"), line);
		if (get_sha1_hex(line, sha1))
			die(_("checkout worker: invalid SHA-1 %s"), line);
		if (line[40] != ' ')
			die(_("checkout worker: whitespace missing %s"), line);
		line += 41;
		len -= 41;
		if (!ce || ce_namelen(ce) < len) {
			free(ce);
			ce = xcalloc(1, cache_entry_size(len));
			ce->ce_mode = S_IFREG | ce_permissions(0644);
		}
		ce->ce_namelen = len;
		hashcpy(ce->sha1, sha1);
		memcpy(ce->name, line, len + 1);

		ret = write_entry(ce, ce->name, &state, 0);
		if (ret) {
			report_pending_ok(&pending_ok);
			packet_write(1, "RET %d", ret);
			continue;
		}
		pending_ok++;
	}
}

static int receive_from_worker(struct worker_process *wp, int revents, void *data)
{
	struct checkout_worker *worker = (struct checkout_worker *)wp;
	struct parallel_checkout *pc = data;
	int fd = worker->wp.cp.out;
	int len, val;
	const char *line;

	if (revents & (POLLERR | POLLNVAL)) {
		pc->errs++;
		return 0;
	}

	line = packet_read_line(fd, &len);
	if (!line) {
		assert(worker->to_send == NULL);
		assert(worker->to_complete == NULL);
		close_and_clear(&worker->wp.cp.out);
		finish_worker(pc, worker);
		return 0;
	}

	if (skip_prefix(line, "RET ", &line)) {
		val = atoi(line);
		if (val >= 0)
			die(_("invalid value %s from checkout worker"), line);
		worker->to_complete = worker->to_complete->next;
		pc->errs++;
	} else if (skip_prefix(line, "OK ", &line)) {
		val = atoi(line);
		if (val <= 0)
			die(_("invalid value %s from checkout worker"), line);

		while (val && worker->to_complete &&
		       worker->to_complete != worker->to_send) {
			if (pc->state.refresh_cache) {
				struct stat st;
				struct cache_entry *ce = worker->to_complete->ce;

				lstat(ce->name, &st);
				fill_stat_cache_info(ce, &st);
				ce->ce_flags |= CE_UPDATE_IN_BASE;
				pc->state.istate->cache_changed |= CE_ENTRY_CHANGED;
			}
			worker->to_complete = worker->to_complete->next;
			progress_one(pc);
			val--;
		}
		if (val)
			die(_("checkout worker reports %d more "
			      "than actual items to complete"), val);
	} else
		die(_("unrecognized response %s from checkout worker"), line);

	return POLLIN;
}

static int write_entries(struct parallel_checkout *pc)
{
	int i, ret = 0;

	for (i = 0; i < pc->nr_items; i++) {
		ret += write_entry(pc->items[i].ce,
				   pc->items[i].ce->name,
				   &pc->state, 0);
		progress_one(pc);
	}

	return ret;
}

int run_parallel_checkout(int nr_workers, int min_limit,
			  int progress_count, struct progress *progress)
{
	struct parallel_checkout *pc = parallel_checkout;
	struct worker_process *wp;
	int ret;

	if (!pc || !pc->nr_items) {
		free(pc);
		parallel_checkout = NULL;
		return 0;
	}

	pc->progress_count = progress_count;
	pc->progress = progress;

	if (pc->nr_items < min_limit) {
		ret = write_entries(pc);
		goto done;
	}

	pc->nr_workers = nr_workers;
	ret = setup_workers(pc);
	if (ret)
		goto done;

	ret = poll_process_pool(&pc->pp);

	if (!ret && pc->errs)
		ret = -1;

done:
	wp = pc->pp.worker;
	while (wp) {
		struct worker_process *next = wp->next;
		finish_worker(pc, (struct checkout_worker *)wp);
		wp = next;
	}

	free(pc->items);
	free(pc);
	parallel_checkout = NULL;
	return ret;
}
