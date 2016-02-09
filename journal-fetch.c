#include "git-compat-util.h"
#include "cache.h"
#include "remote.h"
#include "http.h"
#include "exec_cmd.h"
#include "run-command.h"
#include "journal.h"
#include "progress.h"
#include "refs.h"
#include "journal-common.h"
#include "journal-client.h"
#include "journal-update-ref.h"
#include "journal-util.h"
#include "string-list.h"
#include "commit.h"
#include "hashmap.h"
#include "strbuf.h"
#include "argv-array.h"
#include "sigchain.h"
#include "safe-append.h"

#include <err.h>
#include <sys/file.h>

#define URL_MAX 3096
#define TIP_NAME_AVG 127

static size_t tips_extracted_count;
static int show_progress;
static int no_progress;
static int no_extract;
static int quiet;
static int verbose;
static int disable_fetch_head;
static int no_detach;
static int mirror;
static struct string_list refspecs = STRING_LIST_INIT_DUP;

struct journal_extraction_ctx {
	int journal_fd;
	struct journal_extent_rec *rec;
	struct remote *remote;
	off_t gc_basis;
	struct journal_header header;
};

static struct hashmap fetch_head_table;

struct epilogue_entry {
	struct argv_array command;
	struct strbuf stdin_path;
	unsigned ignore_exit_code: 1;
};

struct epilogue_t {
	size_t alloc;
	size_t count;
	struct epilogue_entry *entries;
};

static struct epilogue_t epilogue;

struct ref_update_ctx {
	struct ref_transaction *transaction;
	struct ref_op_ctx *updates;
	int update_log_fd;
	struct remote *remote;
	struct remote_state *remote_state;
	int prev_back_up_ref_index;
};

static int valid_fd_predicate(const int fd)
{
    return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

static void extent_rec_print(struct journal_extent_rec *r)
{
	printf("Extent record: %"PRIx32"@%"PRIu32"+%"PRIu32"\n",
		     r->serial, r->offset, r->length);
}

static void ref_context_open(struct ref_update_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->updates = ref_op_ctx_create();
	ctx->update_log_fd = open(git_path("journal-fetch-ref-update.log"), O_CREAT|O_APPEND|O_WRONLY, 0600);

	if (ctx->update_log_fd < 0)
		die_errno("failed to open ref update log");
	ctx->prev_back_up_ref_index = -1;
}

static struct ref_update_ctx *ref_context_singleton(void)
{
	static struct ref_update_ctx ctx;
	static int once = 0;

	if (!once) {
		ref_context_open(&ctx);
		once = 1;
	}
	return &ctx;
}

static void log_ref_update_fail(struct ref_update_ctx *update_ctx, struct ref_map_entry *entry)
{
	struct strbuf log = STRBUF_INIT;
	struct strbuf fail_ref = STRBUF_INIT;

	/*
	 * Store the failed ref to the failed-updates dir.  We want to
	 * generate a unique ref name for the failed update, so we
	 * include the hex of the sha1 of the value.
	 */

	strbuf_addf(&fail_ref, "refs/failed-updates/%s/failed-%s",
		    update_ctx->remote->name, sha1_to_hex(entry->sha1));
	update_ref(NULL, fail_ref.buf, entry->sha1, NULL, REF_NODEREF,
		   UPDATE_REFS_DIE_ON_ERR);
	strbuf_release(&fail_ref);

	ref_map_entry_to_strbuf(entry, &log, ' ');
	strbuf_addstr(&log, " failed\n");
	write_in_full(update_ctx->update_log_fd, log.buf, log.len);

	strbuf_release(&log);
}

static int log_ref_update(struct ref_op_ctx *ctx, struct ref_map_entry *entry, void *data)
{
	struct strbuf log = STRBUF_INIT;
	struct ref_update_ctx *update_ctx = (struct ref_update_ctx *)data;

	ref_map_entry_to_strbuf(entry, &log, ' ');
	strbuf_addstr(&log, " succeeded\n");
	write_in_full(update_ctx->update_log_fd, log.buf, log.len);
	strbuf_release(&log);

	return 0;
}

static int ref_context_write_ops(struct ref_op_ctx *ctx, struct ref_map_entry *e, void *data)
{
	struct ref_update_ctx *update_ctx = (struct ref_update_ctx *)data;
	struct strbuf err = STRBUF_INIT;

	if (!update_ctx->transaction) {
		update_ctx->transaction = ref_transaction_begin(&err);
		assert(update_ctx->transaction);
	}

	if (ref_transaction_update(update_ctx->transaction, e->name,
				   e->sha1, NULL, 0,
				   "journal-fetch: forced-update", &err) < 0) {
		/*
		 * This should never happen, because
		 * ref_transaction_update does not consider existing
		 * ref data; a failure here indicates that the journal
		 * contains bad data.  We'll still apply as much of
		 * the update as we can.
		 */
		warning("Failed to update %s: %s", e->name, err.buf);
		strbuf_release(&err);
		log_ref_update_fail(update_ctx, e);
		return 1;
	}

	return 0;
}

static int back_up_outgoing_ref(struct ref_op_ctx *ctx, struct ref_map_entry *e, void *data)
{
	struct ref_update_ctx *update_ctx = (struct ref_update_ctx *)data;
	struct strbuf err = STRBUF_INIT;
	struct strbuf backup_ref = STRBUF_INIT;
	unsigned char sha1[20];
	int ret = 0;
	int index;

	if (!update_ctx->transaction) {
		update_ctx->transaction = ref_transaction_begin(&err);
		assert(update_ctx->transaction);
	}

	if (read_ref(e->name, sha1))
		/* Nothing to back up -- the ref does not exist locally */
		return 0;

	index = ++update_ctx->prev_back_up_ref_index;
	strbuf_addf(&backup_ref, "refs/to-be-deleted/%s/%d",
		    update_ctx->remote->name, index);

	if (ref_transaction_update(update_ctx->transaction, backup_ref.buf,
				   sha1, NULL, 0,
				   NULL, &err) < 0) {
		warning("Failed to update %s: %s", e->name, err.buf);
		strbuf_release(&err);
		log_ref_update_fail(update_ctx, e);
		ret = 1;
		goto done;
	}

done:
	strbuf_release(&backup_ref);
	return ret;
}

static int update_max_index(const char *refname, const struct object_id *oid,
			    int flags, void *cb_data)
{
	struct ref_update_ctx *update_ctx = cb_data;
	char *ep;
	int nr;

	nr = strtol(refname, &ep, 10);
	if (ep > refname) {
		if (nr > update_ctx->prev_back_up_ref_index)
			update_ctx->prev_back_up_ref_index = nr;
	} else {
		warning("Unexpected ref name %s. Ignoring.", refname);
	}
	return 0;
}

static void find_first_back_up_ref_index(struct ref_update_ctx *update_ctx)
{
	struct strbuf prefix = STRBUF_INIT;

	strbuf_addf(&prefix, "refs/to-be-deleted/%s/",
		    update_ctx->remote->name);
	for_each_ref_in(prefix.buf, update_max_index, update_ctx);
	strbuf_release(&prefix);
}

static int delete_one_ref(const char *refname, const struct object_id *sha1,
		      int flags, void *cb_data)
{
	struct strbuf err = STRBUF_INIT;
	struct ref_update_ctx *update_ctx = cb_data;

	/* If this fails, we'll just go ahead and try the rest */
	if (ref_transaction_delete(update_ctx->transaction, refname, NULL,
				   0, "", &err))
		warning("Failed to mark %s for deletion", refname);

	strbuf_release(&err);
	return 0;
}

static void clear_back_up_refs(struct ref_update_ctx *update_ctx)
{
	struct strbuf prefix = STRBUF_INIT;
	struct strbuf err = STRBUF_INIT;

	update_ctx->transaction = ref_transaction_begin(&err);

	strbuf_addf(&prefix, "refs/to-be-deleted/%s/",
		    update_ctx->remote->name);
	for_each_fullref_in(prefix.buf, delete_one_ref, update_ctx, 1);

	if (ref_transaction_commit(update_ctx->transaction, &err))
		warning("Failed to delete back-up refs: %s", err.buf);

	strbuf_release(&prefix);
	strbuf_release(&err);
}

static int ref_context_close(struct ref_update_ctx *update_ctx)
{
	int ret = 0;
	struct strbuf err = STRBUF_INIT;

	/*
	 * Git's ref update API applies ref updates in the order
	 * create/update, then delete.  This ensures that objects
	 * remain referenced at all times during the ref-update
	 * process. Unfortunately, this leads to an increase in d/f
	 * conflicts, since an incoming delete that could clear out a
	 * dir to make space for a conflicting file won't be applied
	 * until too late.  We circumvent this problem (and thus this
	 * protection) by manually applying deletes in one transaction
	 * followed by updates in a second transaction.
	 *
	 * To prevent accidentally gcing needed objects, we instead
	 * back up all too-be-deleted refs to a separate hierarchy
	 * (refs/to-db-deleted/origin).  We use sequentially named
	 * files to prevent d/f conflicts; these aren't user-facing so
	 * we don't have to care what they're called. Then we can
	 * safely process deletes.  Once the creates and updates are
	 * processed, we can remove the backed-up refs.  In the event
	 * of a crash after the deletes but before the updates, the
	 * backed-up refs will prevent a rogue git gc from destroying
	 * data. After any successful fetch, we can remove all backup
	 * files for that remote.
	*/

	find_first_back_up_ref_index(update_ctx);

	if (!update_ctx->transaction) {
		update_ctx->transaction = ref_transaction_begin(&err);
		assert(update_ctx->transaction);
	}

	if (ref_op_ctx_each_op(update_ctx->updates,
			       REF_OP_KIND_DELETE,
			       back_up_outgoing_ref,
			       update_ctx) != 0) {
		warning("backing up existing refs failed");
		ret = 1;
	}

	if (ref_op_ctx_each_op(update_ctx->updates,
			       REF_OP_KIND_DELETE,
			       ref_context_write_ops,
			       update_ctx) != 0)
		die("ref op write failed");

	if (ref_transaction_commit(update_ctx->transaction, &err)) {
		write_in_full(update_ctx->update_log_fd, err.buf, err.len);
		close(update_ctx->update_log_fd);
		die("Failed to commit ref transaction: %s", err.buf);
	}

	ref_transaction_free(update_ctx->transaction);
	update_ctx->transaction = ref_transaction_begin(&err);

	ref_op_ctx_each_op(update_ctx->updates,
			   REF_OP_KIND_DELETE,
			   log_ref_update,
			   update_ctx);

	/* Write creates and updates. */
	if (ref_op_ctx_each_op(update_ctx->updates,
			       REF_OP_KIND_CREATE | REF_OP_KIND_UPDATE,
			       ref_context_write_ops,
			       update_ctx) != 0)
		die("ref op write failed");

	if (ref_transaction_commit(update_ctx->transaction, &err)) {
		write_in_full(update_ctx->update_log_fd, err.buf, err.len);
		close(update_ctx->update_log_fd);
		die("Failed to commit ref transaction: %s", err.buf);
	}

	ref_op_ctx_each_op(update_ctx->updates,
			   REF_OP_KIND_CREATE | REF_OP_KIND_UPDATE,
			   log_ref_update,
			   update_ctx);

	if (update_ctx->update_log_fd > 0)
		close(update_ctx->update_log_fd);

	ref_transaction_free(update_ctx->transaction);
	update_ctx->transaction = NULL;

	clear_back_up_refs(update_ctx);

	ref_op_ctx_destroy(update_ctx->updates);
	return ret;
}

enum epilogue_states { EPILOGUE_UNSTARTED, EPILOGUE_STARTED, EPILOGUE_FINISHED };
static enum epilogue_states epilogue_state = EPILOGUE_UNSTARTED;

static void maybe_detach(int return_code)
{
	if (no_detach) {
		struct strbuf message = STRBUF_INIT;

		strbuf_addf(&message,
			    Q_("Processing %"PRIuMAX" entry in the foreground.",
			       "Processing %"PRIuMAX" entries in the foreground.",
			       (uintmax_t) epilogue.count),
			    epilogue.count);
		puts(message.buf);
		strbuf_release(&message);
	} else {
		const char *epilogue_log_path = git_path("journal-fetch-epilogue.log");
		pid_t fork_result;
		int log_fd;
		const char *merge_pid = getenv("GIT_PULL_PID");

		trace_printf("(Processing will continue in the background -- logging to '%s'.)\n",
			epilogue_log_path);
		fork_result = fork();
		if (fork_result == 0)
			daemonize();
		else if (fork_result > 0)
			exit(return_code);
		else
			warning("Failed to daemonize; continuing in foreground");
		log_fd = open(epilogue_log_path, O_RDWR|O_TRUNC|O_CREAT|O_APPEND, 0600);
		dup2(log_fd, STDOUT_FILENO);
		dup2(log_fd, STDERR_FILENO);
		close(log_fd);

		if (merge_pid) {
			int finished = 0;
			size_t max_loops = 300;
			size_t loops;
			long merge_pid_long;

			errno = 0;
			merge_pid_long = strtol(merge_pid, NULL, 10);

			if (!merge_pid_long && errno)
				die("parsing GIT_PULL_PID failed");
			for (loops = max_loops; loops > 0; loops--) {
				errno = 0;
				if (kill((int)merge_pid_long, 0) != 0 && errno == ESRCH) {
					finished = 1;
					break;
				}
				if (loops == max_loops || loops % 10 == 0)
					printf("Merge is still running as PID %ld, continuing to wait up to %zu seconds for it to complete.\n",
						   merge_pid_long, loops);
				sleep(1);
			}

			if (finished)
				printf("Merge finished, continuing.\n");
			else
				printf("Timed out waiting for merge to complete, continuing with abandon.\n");
		} else {
			printf("Not waiting for a merge to finish.\n");
		}
	}
}

static void epilogue_perform(int return_code)
{
	struct progress *progress = NULL;
	size_t i, j;
	int failing = 0;
	uint64_t start = getnanotime();

	if (epilogue_state != EPILOGUE_UNSTARTED)
		return;
	epilogue_state = EPILOGUE_STARTED;

	maybe_detach(return_code);

	if (show_progress) {
		progress = start_progress("Processing epilogue", epilogue.count);
	}

	printf("Epilogue contains %zu items.\n", epilogue.count);

	for (i = 0; i < epilogue.count && !failing; ++i) {
		int fd = -1;
		struct epilogue_entry *e = &epilogue.entries[i];
		struct child_process child;
		uint64_t epilogue_child_start = getnanotime();
		int r = -128;

		if (progress)
			display_progress(progress, i);

		start = getnanotime();
		memset(&child, 0, sizeof(child));
		child.argv = e->command.argv;
		child.in = -1;
		child.git_cmd = 0;

		if (verbose) {
			/* Write a diagnostic message */
			struct strbuf msg = STRBUF_INIT;

			strbuf_addf(&msg, "-- Epilogue (%zu of %zu): starting '",
				(i+1), epilogue.count);
			for (j = 0; j < e->command.argc; ++j) {
				strbuf_addf(&msg, (e->command.argc - j > 1) ? "%s " : "%s",	e->command.argv[j]);
			}
			strbuf_addf(&msg, "' (stdin:%s)\n", e->stdin_path.buf);
			write_in_full(STDOUT_FILENO, msg.buf, msg.len);
			fflush(stdout);
			strbuf_release(&msg);
		}

		/* If specified, open the associated file for use as stdin */
		if (e->stdin_path.len > 0) {
			struct stat sb;
			if ((stat(e->stdin_path.buf, &sb) != 0) || !S_ISREG(sb.st_mode)) {
				warning("Epilogue: tried to open '%s' as stdin, but that path is not a regular file; skipping this entry.", e->stdin_path.buf);
				failing = 1;
				goto finish2;
			}

			fd = open(e->stdin_path.buf, O_RDONLY);

			if (fd < 0) {
				warning("Epilogue: tried to open '%s' as stdin, but open() failed: %s", e->stdin_path.buf, strerror(errno));
				failing = 1;
				goto finish2;
			} else {
				child.in = fd;
			}
		}

		/* Start the command */
		if (start_command(&child) < 0) {
			warning("Epilogue: unable to start command");
			failing = 1;
			goto finish2;
		}

		/* Wait for it to end */
		r = finish_command(&child);
		if (r != 0){
			if (!e->ignore_exit_code) {
				failing = 1;
			}
			warning("Epilogue: command exited dirty (%d): %s",
					r, failing ? "marking a failure" :
				"ignoring the failure");
		}

	finish2:
		if (fd > -1 && valid_fd_predicate(fd)) {
			if (close(fd) != 0)
				warning("Epilogue: failed to close FD %d: %s", fd, strerror(errno));
		}

		trace_performance(epilogue_child_start, "epilogue_child");
		if (failing)
			warning("Epilogue: command failed");
		argv_array_clear(&e->command);
		strbuf_release(&e->stdin_path);
	}

	if (progress) {
		display_progress(progress, i);
		stop_progress(&progress);
	}

	trace_performance_since(start, "epilogues");
	epilogue_state = EPILOGUE_FINISHED;
}

/* Append an epilogue action _item_. A copy will be made. */
static struct epilogue_entry *epilogue_append(void)
{
	struct epilogue_entry *e;

	ALLOC_GROW(epilogue.entries, epilogue.count + 1, epilogue.alloc);
	e = &epilogue.entries[epilogue.count++];
	memset(e, 0, sizeof(*e));
	argv_array_init(&e->command);
	strbuf_init(&e->stdin_path, 8);
	return e;
}

/* Schedule combine-pack operation. */
static void schedule_repack(void)
{
	struct epilogue_entry *e = epilogue_append();
	char ubstr[20];
	size_t upper_bound;

	upper_bound = git_config_ulong("combinepack.size-limit", "134217728");

	e->ignore_exit_code = 1;
	argv_array_push(&e->command, "git");
	argv_array_push(&e->command, "combine-pack");
	argv_array_push(&e->command, "--size-upper-bound");
	snprintf(ubstr, sizeof(ubstr), "%"PRIuMAX, (uintmax_t) upper_bound);
	argv_array_push(&e->command, ubstr);
}

/* Schedule a post-journal-epilogue hook */
static void schedule_epilogue_hook(void)
{
	struct epilogue_entry *e;
	const char *hook_path = find_hook("post-journal-fetch");

	if (hook_path) {
		e = epilogue_append();
		e->ignore_exit_code = 1;
		argv_array_push(&e->command, hook_path);
		if (no_extract) {
			argv_array_push(&e->command, "--no-extract");
		}
	}
}

static char const * const journal_fetch_usage[] = {
	N_("git journal-fetch --remote <remote_name>"),
	NULL
};

struct fetch_head_table_entry {
	struct hashmap_entry entry;
	char *line;
};

static void write_initial_fetch_head(struct remote *remote, const char *name);

static int fetch_rest_of_file(const char *repo_url,
			      const char *specific_url,
			      const char *target_path,
			      off_t discarded)
{
	struct strbuf url = STRBUF_INIT;
	struct strbuf exp = STRBUF_INIT;
	struct strbuf type = STRBUF_INIT;
	struct strbuf buffer = STRBUF_INIT;
	struct strbuf effective_url = STRBUF_INIT;

	struct http_get_options options;
	int http_ret;

	memset(&options, 0, sizeof(options));
	options.content_type = &type;
	options.effective_url = &effective_url;
	strbuf_addstr(&url, repo_url);
	options.base_url = &url;
	options.no_cache = 0;
	options.display_progress = show_progress;

	http_ret = http_resume_file(specific_url, target_path, &options, discarded);
	switch (http_ret) {
	case HTTP_OK:
	case HTTP_RANGE_NOT_SATISFIABLE:
		break;
	case HTTP_MISSING_TARGET:
		die("Repository '%s' not found", url.buf);
	case HTTP_NOAUTH:
		die("Authentication failed for '%s'", url.buf);
	default:
		die("unable to access '%s': %s", url.buf, curl_errorstr);
	}

	strbuf_release(&exp);
	strbuf_release(&type);
	strbuf_release(&effective_url);
	strbuf_release(&buffer);

	return http_ret;
}

static int fetch_extents(struct remote *upstream, struct remote_state *r, const char *repo_url)
{
	char extents_url[URL_MAX];
	const char *ext_path;
	struct stat sb;
	size_t updated_bytes = 0;
	int ext_ret;
	int lock_fd = -1;
	uint64_t start = getnanotime();

	http_init(upstream, repo_url, 1);
	snprintf((char *)&extents_url, sizeof(extents_url),
		 "%s/objects/journals/extents.bin",
		 repo_url);
	ext_path = extents_path(upstream);

	lock_fd = open_safeappend_file(ext_path, O_CREAT | O_RDONLY, 0644);
	if (lock_fd < 0)
		die_errno("Unable to create file '%s'", ext_path);
	if (flock(lock_fd, LOCK_EX | LOCK_NB)) {
		if (errno == EWOULDBLOCK)
			die("Extents already being fetched by another process.");
		die_errno("Unable to lock for fetch_extents, lock file '%s'", ext_path);
	}

	if (stat(ext_path, &sb) == 0)
		updated_bytes = xsize_t(sb.st_size);
	ext_ret = fetch_rest_of_file(repo_url, extents_url, ext_path, 0);
	if (stat(ext_path, &sb) != 0)
		die_errno("Could not stat extents %s", ext_path);
	updated_bytes = xsize_t(sb.st_size) - updated_bytes;
	r->rec_count = xsize_t(sb.st_size) / sizeof(struct journal_extent_rec);
	if (updated_bytes > 0) {
		int transaction_count = updated_bytes / sizeof(struct journal_extent_rec);
		if (!quiet) {
			printf(Q_("%s: Extents updated: %d transaction\n",
				  "%s: Extents updated: %d transactions\n",
				  transaction_count),
			       upstream->name, transaction_count);
		}
	} else {
		if (!quiet) {
			printf("%s: Extents already up-to-date\n",
			       upstream->name);
		}
	}

	if (flock(lock_fd, LOCK_UN))
		die_errno("Could not unlock '%s'", ext_path);
	/*
	 * Since we opened the file read-only, we don't need to change
	 * the size file, so we can use close instead of commit_safeappend_file.
	 */
	close(lock_fd);

	trace_performance_since(start, "extents");
	return ext_ret;
}

/*
 * Unsafe if called outside of fetch_and_process_journals (fetch must be
 * exclusive)
 */
static int fetch_journal(struct remote *upstream, const char *repo_url,
			 struct journal_extent_rec *rec, off_t discarded,
			 const char *path)
{
	char url[URL_MAX];
	int r;
	const off_t ext_end = rec->offset + rec->length;
	uint64_t start = getnanotime();

	snprintf(url, sizeof(url), "%s/objects/journals/%"PRIx32".bin",
		 repo_url, rec->serial);

	if (!quiet) {
		printf("%s: Updating from %s\n",
		       upstream->name, url);
	}
	r = fetch_rest_of_file(repo_url, url, path, discarded);

	if (r != HTTP_OK) {
		die("Failed to fetch journal %s through %"PRIuMAX" bytes",
		    url,
		    (uintmax_t) ext_end);
	}

	trace_performance(start, "fetch_journals");
	return r;
}

static int journal_read_header(struct journal_extraction_ctx *ctx)
{
	const size_t bytes_read = read_in_full(ctx->journal_fd, &ctx->header, sizeof(ctx->header));

	if (bytes_read != sizeof(ctx->header)) {
		const size_t current_offset = xsize_t(lseek(ctx->journal_fd, 0, SEEK_CUR));

		warning("Read failed: journal header (read %zuB, wanted %zuB) @ %ju",
			bytes_read, sizeof(ctx->header), current_offset);
		extent_rec_print(ctx->rec);
		return 1;
	}
	journal_header_from_wire(&ctx->header);
	return 0;
}

static const char *read_current_head(void)
{
	static char *data = NULL;
	const char *fn;
	struct stat sb;
	int fd;

	if (data)
		return data;

	fn = git_path("HEAD");

	if (stat(fn, &sb) != 0)
		die_errno("stat failed: %s", fn);
	fd = open(fn, O_RDONLY);
	if (fd < 0)
		die_errno("open failed: %s", fn);
	data = xmalloc(sb.st_size);
	if (read_in_full(fd, (void *)data, sb.st_size) != sb.st_size)
		die_errno("read failed: %s", fn);

	if (data[sb.st_size - 1] == '\n')
		data[sb.st_size - 1] = '\0';
	close(fd);

	return data;
}

static const char *norm_current_head(void)
{
	const char *current_head = read_current_head();
	const char *cm;

	if (!current_head)
		die("could not read HEAD");
	cm = &current_head[sizeof("ref: ") - 1];
	if (starts_with(cm, "refs/heads/"))
		return &cm[sizeof("refs/heads/") - 1];
	else if (starts_with(cm, "refs/tags/"))
		return &cm[sizeof("refs/tags/") - 1];
	else
		die("unable to parse current refname");
}

static int current_ref_predicate(const char *ref_name)
{
	const char *current_head;
	const char *cm;

	current_head = read_current_head();
	cm = &current_head[sizeof("ref: ") - 1];
	if (starts_with(current_head, "ref: "))
		return (!strcmp(cm, ref_name));
	return 0;
}

static int tag_predicate(const char *name)
{
	return starts_with(name, "refs/tags/");
}

static int desired_ref_predicate(const char *ref_name)
{
	return unsorted_string_list_has_string(&refspecs, ref_name);
}

static void enqueue_fetch_head(const char *tip_name,
		const char *normalized_tip_name,
		const unsigned char *new_sha,
		const char *remote_url)
{
	struct strbuf fetch_head_line = STRBUF_INIT;
	int tag = tag_predicate(tip_name);
	const char *name_for_fetch_head;
	struct fetch_head_table_entry *te, *tp;

	strbuf_addstr(&fetch_head_line, sha1_to_hex(new_sha));
	strbuf_addch(&fetch_head_line, '\t');

	if (tag) {
		name_for_fetch_head = &tip_name[sizeof("refs/tags/") - 1];
		/* xxx - check whether tags can be merged */
		strbuf_addstr(&fetch_head_line, "not-for-merge\ttag '");
		strbuf_addstr(&fetch_head_line, name_for_fetch_head);
		strbuf_addstr(&fetch_head_line, "' of ");
		strbuf_addstr(&fetch_head_line, remote_url);
		strbuf_addch(&fetch_head_line, '\n');
	} else {
		name_for_fetch_head = &tip_name[sizeof("refs/heads/") - 1];
		if (current_ref_predicate(tip_name) || desired_ref_predicate(normalized_tip_name)) {
			strbuf_addstr(&fetch_head_line, "\tbranch '");
		} else {
			strbuf_addstr(&fetch_head_line, "not-for-merge\tbranch '");
		}
		strbuf_addstr(&fetch_head_line, name_for_fetch_head);
		strbuf_addstr(&fetch_head_line, "' of ");
		strbuf_addstr(&fetch_head_line, remote_url);
		strbuf_addch(&fetch_head_line, '\n');
	}

	te = xcalloc(1, sizeof(*te));
	hashmap_entry_init(te, memhash(name_for_fetch_head, strlen(name_for_fetch_head)));
	te->line = xstrdup(fetch_head_line.buf);

	tp = hashmap_put(&fetch_head_table, te);
	if (tp) {
		if (tp->line)
			free(tp->line);
		free(tp);
	}
}

static int write_ref_updates(void)
{
	static int refs_updated = 0;
	int ret;
	uint64_t start = getnanotime();

	if (refs_updated)
		return 0;
	refs_updated = 1;

	ret = ref_context_close(ref_context_singleton());
	trace_performance(start, "write_ref_update");
	return ret;
}

static int journal_extract_tip(struct journal_extraction_ctx *ctx)
{
	char tip_name[PATH_MAX];
	const size_t tip_name_len = ctx->header.payload_length;
	struct ref_update_ctx *update_ctx;
	struct strbuf normalized_name = STRBUF_INIT;
	uint64_t start = getnanotime();

	update_ctx = ref_context_singleton();

	/* Read ref name */
	if (tip_name_len >= PATH_MAX - 1) {
		warning("impossibly long ref name");
		return -1;
	}

	errno = 0;
	if (read_in_full(ctx->journal_fd, tip_name, tip_name_len) != tip_name_len) {
		const char *err;
		if (errno)
			err = strerror(errno);
		else
			err = "not enough data";
		warning("read failed: journal entry ref name: %s", err);
		return -1;
	}
	tip_name[tip_name_len] = '\0';

	if (!ctx->remote->mirror && starts_with(tip_name, "refs/heads/")) {
		const char *rsuffix = &tip_name[sizeof("refs/heads/") - 1];
		strbuf_addf(&normalized_name, "refs/remotes/%s/%s",
				ctx->remote->name, rsuffix);
	} else {
		strbuf_add(&normalized_name, tip_name, tip_name_len);
	}

	if (!disable_fetch_head)
		enqueue_fetch_head(tip_name, normalized_name.buf, ctx->header.sha, ctx->remote->url[0]);

	ref_op_add(update_ctx->updates, normalized_name.buf,
		   normalized_name.len, ctx->header.sha);

	strbuf_release(&normalized_name);
	++tips_extracted_count;
	trace_performance(start, "journal_extract_tip");

	return 0;
}

static size_t journal_limit(struct journal_extraction_ctx *ctx)
{
	return (ctx->rec->offset - ctx->gc_basis) + ctx->rec->length;
}

static void journal_extract_segment(struct journal_extraction_ctx *ctx, int fd)
{
	static struct progress *progress;
	size_t limit;
	/* Read the payload */
	off_t here;
	size_t nr_bytes;
	unsigned char *buf;
	size_t copied = 0;
	uint64_t start = getnanotime();

	limit = journal_limit(ctx);
	here = lseek(ctx->journal_fd, 0, SEEK_CUR);
	nr_bytes = limit - here;

	buf = xmalloc(journal_buf_size);

	if (show_progress)
		progress = start_progress("Extracting journal", nr_bytes);

	while (copied < nr_bytes) {
		ssize_t remain = nr_bytes - copied;
		size_t stride = (remain < journal_buf_size) ? remain : journal_buf_size;
		ssize_t loaded = read_in_full(ctx->journal_fd, buf, stride);

		if (loaded != stride)
			die_errno("read failed: buffered journal payload");

		if (write_in_full(fd, buf, loaded) != loaded)
			die_errno("write failed: buffered journal payload");

		copied += loaded;

		if (progress)
			display_throughput(progress, copied);
	}
	if (progress)
		stop_progress(&progress);
	free(buf);
	trace_performance(start, "extract_records");
}

static int pack_exists_predicate(const unsigned char *sha)
{
	struct stat sb;
	const char *p = mkpath("%s/pack/pack-%s.pack", get_object_directory(),
			       sha1_to_hex(sha));
	return stat(p, &sb) == 0;
}

static void skip_entry(struct journal_extraction_ctx *ctx)
{
	const size_t limit = journal_limit(ctx);
	off_t here = lseek(ctx->journal_fd, limit, SEEK_SET);

	if (here != limit)
		die_errno("seek failed");
}

static int journal_extract_file(struct journal_extraction_ctx *ctx)
{
	char tmp_name[PATH_MAX];
	char final_name[PATH_MAX];

	if (ctx->rec->opcode == JOURNAL_OP_PACK) {
		int keep_fd = 0;

		keep_fd = odb_pack_keep(tmp_name, sizeof(tmp_name), ctx->header.sha);
		if (keep_fd < 0)
			die_errno("failed to create pack keep file");
		snprintf(final_name, sizeof(final_name), "%s/pack/pack-%s.pack",
			 get_object_directory(), sha1_to_hex(ctx->header.sha));

		journal_extract_segment(ctx, keep_fd);

		if (close(keep_fd) != 0)
			die_errno(_("cannot close written keep file '%s'"),
				  tmp_name);

	} else if (ctx->rec->opcode == JOURNAL_OP_INDEX) {
		/* Require that the pack exists */
		int pack_exists = pack_exists_predicate(ctx->header.sha);
		int idx_fd;

		if (pack_exists == 0) {
			warning("Index %s without pack; skipping.", sha1_to_hex(ctx->header.sha));
			skip_entry(ctx);
			return 0;
		}

		idx_fd = odb_mkstemp(tmp_name, sizeof(tmp_name),
					 "pack/tmp_journaled_pack_index_XXXXXX");
		snprintf(final_name, sizeof(final_name), "%s/pack/pack-%s.idx",
			 get_object_directory(), sha1_to_hex(ctx->header.sha));

		journal_extract_segment(ctx, idx_fd);

		if (close(idx_fd) != 0)
			die_errno("cannot close written pack index");

	} else {
		warning("unhandled file extraction with opcode 0x%02x",
			ctx->rec->opcode & 0xff);
		return -1;
	}

	if (finalize_object_file(tmp_name, final_name))
		die("cannot move temp file '%s' into place as '%s'",
		    tmp_name, final_name);

	if (mirror && ctx->rec->opcode == JOURNAL_OP_INDEX) {
		struct packed_git *pack;

		pack = journal_find_pack(ctx->header.sha);
		assert(pack);
	}
	return 0;
}

static int journal_check_upgrade(struct journal_extraction_ctx *ctx)
{
	const struct journal_wire_version *other_ver;
	const struct journal_wire_version *my_ver;
	uint16_t other_version;
	uint16_t my_version;

	other_ver = (const struct journal_wire_version *)ctx->header.sha;
	other_version = ntohs(other_ver->version);

	my_ver = journal_wire_version();
	my_version = ntohs(my_ver->version);

	if (other_version > my_version) {
		warning("Your Git client must be upgraded.\n"
			"  The journal requires wire protocol version %"PRIu16".\n"
			"  This client supports wire protocol version %"PRIu16".",
			other_version, my_version);
		return -1;
	}

	return 0;
}

/*
 * Do not advance the processed offset when a pack has been processed.
 * Only advance it for refs
 */
#define DO_NOT_ADVANCE 2

static int journal_extract(struct journal_extraction_ctx *ctx)
{
	struct stat sb;
	off_t to;

	if (fstat(ctx->journal_fd, &sb) != 0)
		die_errno("stat failed");

	/* Seek to the location indicated in the journal, less the number of bytes trashed. */
	to = ctx->rec->offset - ctx->gc_basis;

	if (lseek(ctx->journal_fd, to, SEEK_SET) != to)
		die_errno("seek failed");

	/* Read the entry. */
	if (journal_read_header(ctx)) {
		return -1;
	}

	if (ctx->header.opcode == JOURNAL_OP_REF) {
		if (journal_extract_tip(ctx))
			return -1;
	} else if (ctx->header.opcode == JOURNAL_OP_PACK ||
		   ctx->header.opcode == JOURNAL_OP_INDEX) {
		journal_extract_file(ctx);
		return DO_NOT_ADVANCE;
	} else if (ctx->header.opcode == JOURNAL_OP_UPGRADE) {
		if (journal_check_upgrade(ctx))
			return -1;
	} else {
		die("Unsupported opcode (0x%02x)",
		    ctx->header.opcode & 0xff);
	}

	return 0;
}

static void warn_extents_past_journal(uint32_t offset, uint32_t length,
				      size_t journal_size)
{
	trace_printf(
		"An extents record at %"PRIu32", length %"PRIu32
		" is beyond the currently downloaded journal length %"PRIuMAX
		"\n",
		offset, length,	(uintmax_t) journal_size);
}

static const char *journal_path(struct remote *r, const char *fmt, ...)
{
	static struct strbuf sb = STRBUF_INIT;
	va_list ap;

	strbuf_reset(&sb);
	strbuf_addstr(&sb, journal_remote_dir(r));
	strbuf_addch(&sb, '/');
	va_start(ap, fmt);
	strbuf_vaddf(&sb, fmt, ap);
	va_end(ap);

	return sb.buf;
}

static int fetch_and_process_journals(struct remote *upstream,
		const char *repo_url,
		struct remote_state *r)
{
	int ret = 0, ext_fd, fd = -1, aborted = 0;
	const char *ext_path = extents_path(upstream);
	uint32_t serial = 0;
	size_t i;
	size_t extent_rec_count;
	size_t journal_size = 0;
	off_t processed_offset_at_start;
	struct progress *progress = NULL;
	struct ref_update_ctx *update_ctx;
	struct journal_extent_rec rec = {0};
	off_t discarded = 0;
	const char *path;
	size_t processed_pack_extents = 0;
	uint64_t start;

	if (r->rec_count == 0)
		return 0;

	if (r->r.processed_offset > r->rec_count * sizeof(rec))
		die("%s: Extents processed exceeds extent of extents.",
		    upstream->name);

	assert(r->rec_count >= r->r.processed_offset / sizeof(rec));

	extent_rec_count = r->rec_count - r->r.processed_offset / sizeof(rec);

	if (extent_rec_count == 0)
		return 0;

	ext_fd = open_safeappend_file(ext_path, O_RDONLY, 0);
	if (ext_fd < 0)
		die_errno("open failed: extents file '%s'", ext_path);
	/* Note, this also serves as a lock for journal fetching and processing. */
	if (flock(ext_fd, LOCK_EX | LOCK_NB)) {
		if (errno == EWOULDBLOCK)
			die("Extents file '%s' locked by another process.", ext_path);
		die_errno("Unable to lock extents file '%s'", ext_path);
	}
	if (lseek(ext_fd, r->r.processed_offset, SEEK_SET) != r->r.processed_offset)
		die_errno("seek failed: extents");

	start = getnanotime();
	prepare_packed_git();
	trace_performance(start, "prepare_packed_git");

	processed_offset_at_start = r->r.processed_offset;

	if (show_progress) {
		const char *kind = (no_extract) ? "Buffering content" : "Replaying transactions";

		progress = start_progress(kind, extent_rec_count);
	}

	update_ctx = ref_context_singleton();
	update_ctx->remote = upstream;
	update_ctx->remote_state = r;

	serial = r->last.serial;
	if (!mirror)
		discarded = r->last.offset + r->last.length;

	for (i = 0; !aborted && i < extent_rec_count; ++i) {
		struct journal_extraction_ctx ctx;

		read_extent_rec(ext_fd, &rec);
		assert (serial <= rec.serial);

		if (verbose)
			extent_rec_print(&rec);

		if (i != 0 && serial == rec.serial &&
		    rec.offset + rec.length > journal_size) {
			/*
			 * The extents file has gone beyond the
			 * available journal.  So we'll declare
			 * ourselves done, and next time we run,
			 * we'll hopefully have more journal
			 * to work with.
			 */
			warn_extents_past_journal(rec.offset, rec.length,
						  journal_size);
			break;
		}

		if (i == 0 || serial != rec.serial) {
			int hret;
			struct stat sb;

			/* Close and delete the current journal */
			if (fd >= 0) {
				if (mirror)
					commit_safeappend_file(path, fd);
				else if (!no_extract)
					unlink_safeappend_file(path);
				close(fd);
			}

			/* Changing to a new journal */
			if (serial != rec.serial) {
				serial = rec.serial;
				discarded = 0;
			}

			if (i == 0 || serial != rec.serial) {
				path = journal_path(upstream, "%"PRIx32".bin",
						    rec.serial);
			}

			hret = fetch_journal(upstream, repo_url, &rec,
					     discarded, path);
			if (hret != HTTP_OK && hret != HTTP_RANGE_NOT_SATISFIABLE)
				die_errno("Could not fetch journal %"PRIx32" from %s",
					  serial, repo_url);

			fd = open_safeappend_file(path, O_RDONLY, 0);
			if (fd < 0)
				die_errno("journal open failed: %s", path);

			if (fstat(fd, &sb) == 0)
			       journal_size = discarded + xsize_t(sb.st_size);
			else
				die("failed to stat newly-created journal");

			trace_printf("Opened journal #%"PRIu32" from '%s' FD=%d\n",
				     serial, path, fd);

			if (rec.offset + rec.length > journal_size) {
				/*
				 * Even though we have just downloaded
				 * a new journal chunk, this extents
				 * chunk is still beyond what's in the
				 * journal so far.  This can
				 * potentially happen the innocuous
				 * reason that the extents file on
				 * some read replica is ahead of the
				 * journal file.
				 */
				warn_extents_past_journal(rec.offset,
							  rec.length,
							  journal_size);

				break;
			}

		}

		/* read things out of the journal */
		ctx.gc_basis = discarded;
		ctx.journal_fd = fd;

		ctx.rec = &rec;
		ctx.remote = upstream;
		if (no_extract) {
			if (verbose)
				trace_printf("... Skipping extraction of journal content\n");
		} else {
			int result = journal_extract(&ctx);

			if (!result) {
				r->r.processed_offset += sizeof(struct journal_extent_rec) + processed_pack_extents;
				processed_pack_extents = 0;
			} else if (result == DO_NOT_ADVANCE) {
				processed_pack_extents += sizeof(struct journal_extent_rec);
			} else {
				++aborted;
				ret = result;
				warning("journal extraction terminated abnormally "
					"(journal_extract returned %d, processed through %"PRIu32"@%"PRIx32")",
					result, rec.serial, r->r.processed_offset);
			}

		}

		if (progress)
			display_progress(progress, i);
	}

	if (progress) {
		display_progress(progress, i);
		stop_progress(&progress);
	}

	if (write_ref_updates() != 0)
		warning("journal-fetch: some ref updates failed, but\n"
			"continuing anyway because otherwise you'll miss the\n"
			"updates that do apply. See %s\nfor details.",
			git_path("journal-fetch-ref-update.log"));

	if (!no_extract) {
		if (r->r.processed_offset > processed_offset_at_start) {
			schedule_repack();
		}
	}

	schedule_epilogue_hook();

	if (fd >= 0) {
		if (!ret && !no_extract && !mirror)
			/*
			 * TODO: this might accidentally delete a
			 * trailing partial record, but we'll just
			 * redownload it next time.
			 */
			unlink_safeappend_file(path);
		else if (mirror)
			commit_safeappend_file(path, fd);

		close(fd);
	}
	if (flock(ext_fd, LOCK_UN))
		warning("Failed to unlock '%s'", ext_path);
	close(ext_fd);

	if (!ret && mirror) {
		struct journal_metadata metadata;

		path = journal_path(upstream, "metadata.bin");
		metadata.journal_serial = serial;
		journal_metadata_store(path, &metadata);
	}

	start = getnanotime();
	reprepare_packed_git();
	trace_performance(start, "reprepare_packed_git");

	return ret;
}

static void write_initial_fetch_head(struct remote *remote, const char *name)
{
	unsigned char current_sha[20];
	size_t len;
	struct strbuf ref_name = STRBUF_INIT;
	struct fetch_head_table_entry *tp, *te;
	struct strbuf fetch_head_line = STRBUF_INIT;

	strbuf_addstr(&ref_name, "refs/remotes/");
	strbuf_addstr(&ref_name, remote->name);
	strbuf_addch(&ref_name, '/');
	strbuf_addstr(&ref_name, name);
	if (get_sha1(ref_name.buf, current_sha)) {
		/* Local branches won't have this treatment. */
		trace_printf("didn't resolve likely local ref %s\n", ref_name.buf);
		strbuf_release(&ref_name);
		return;
	}

	strbuf_addstr(&fetch_head_line, sha1_to_hex(current_sha));
	strbuf_addstr(&fetch_head_line, "\t\tbranch '");
	strbuf_addstr(&fetch_head_line, name);
	strbuf_addstr(&fetch_head_line, "' of ");
	strbuf_addstr(&fetch_head_line, remote->url[0]);
	strbuf_addch(&fetch_head_line, '\n');

	te = xcalloc(1, sizeof(*te));
	te->line = strbuf_detach(&fetch_head_line, &len);
	hashmap_entry_init(te, memhash(name, strlen(name)));
	tp = hashmap_put(&fetch_head_table, te);
	if (tp) {
		/* should not happen -- we are before any others */
		if (tp->line)
			free(tp->line);
		free(tp);
	}
}

static int write_fetch_head_element(void *ptr, void *data)
{
	int fd = *(int *)data;
	struct fetch_head_table_entry *e = (struct fetch_head_table_entry *)ptr;
	const char *item = e->line;
	size_t item_len = strlen(item);

	if (write_in_full(fd, item, item_len) != item_len)
		die_errno("write failed: fetch head string[%zu] '%s'",
			  item_len, item);
	free(e->line);
	return 0;
}

static void write_fetch_head_file(struct hashmap *fetch_head_table)
{
	const char *fn;
	int fd;
	const char *cm;
	struct fetch_head_table_entry *e;
	struct hashmap_iter iter;
	uint64_t start = getnanotime();

	fn = git_path("FETCH_HEAD");
	fd = open(fn, O_CREAT | O_TRUNC | O_WRONLY, 0600);

	if (fd < 0)
		die_errno("open failed: %s", fn);

	cm = norm_current_head();

	e = xcalloc(1, sizeof(*e));
	hashmap_entry_init(e, memhash(cm, strlen(cm)));
	/* Write current fetch ref entry first. */
	e = hashmap_remove(fetch_head_table, e, cm);
	if (e) {
		write_fetch_head_element(e, &fd);
	} else {
		/* must be a non-tracking branch */
	}

	/* Write the rest of the entries. */

	hashmap_iter_init(fetch_head_table, &iter);
	while ((e = hashmap_iter_next(&iter)))
		write_fetch_head_element(e, &fd);

	if (close(fd) != 0)
		die_errno("close failed");
	trace_performance(start, "write_fetch_head");
}

static void prepare_directory(struct remote *remote)
{
	struct stat s;
	const char *dir;

	/* stat 'objects' so we make our dirs have the same mode */
	if (stat(get_object_directory(), &s) != 0)
		die_errno("unable to stat directory 'objects'");

	/* make the journals directory if necessary */
	if (mkdir(journal_dir(), s.st_mode) < 0)
		if (errno != EEXIST)
			die_errno("unable to create journal directory");

	/* make the remote-specific directory if necessary */
	dir = journal_remote_dir(remote);
	if (mkdir(dir, s.st_mode) < 0)
		if (errno != EEXIST)
			die_errno("unable to create journal remote directory '%s'",
				  dir);
}

static void create_size_file(const char *name)
{
	struct strbuf size_file = STRBUF_INIT;
	struct stat st;
	int fd;
	uint64_t size;

	strbuf_addstr(&size_file, name);
	strbuf_addstr(&size_file, ".size");

	if (lstat(name, &st)) {
		warning("Could not read %s", name);
		return;
	}

	fd = open(size_file.buf, O_WRONLY|O_CREAT, 0666);
	assert (fd >= 0);
	size = htonll(st.st_size);
	write_or_die(fd, &size, 8);

	close(fd);
	strbuf_release(&size_file);
}

static void journal_size_backfill(struct remote * const remote)
{
	struct stat st;
	const char *journal_dir;
	struct strbuf journal_file = STRBUF_INIT;
	size_t len;
	DIR *dir;
	struct dirent *de;

	journal_dir = journal_remote_dir(remote);
	strbuf_addstr(&journal_file, journal_dir);
	strbuf_addch(&journal_file, '/');
	len = journal_file.len;

	strbuf_addstr(&journal_file, "extents.bin.size");
	if (!stat(journal_file.buf, &st))
		/* extents size file exists, no need to backfill */
		return;

	dir = opendir(journal_dir);
	if (!dir)
		die_errno("Failed to open journal dir %s", journal_dir);

	while ((de = readdir(dir))) {
		strbuf_setlen(&journal_file, len);
		if (ends_with(de->d_name, ".bin") && !ends_with(de->d_name, "state.bin")) {
			strbuf_addstr(&journal_file, de->d_name);
			create_size_file(journal_file.buf);
		}
	}
}

static int journal_fetch(struct remote * const remote)
{
	size_t i;
	size_t failures = 0;
	struct remote_state r;

	prepare_directory(remote);
	remote_state_load(remote, &r, 0);

	journal_size_backfill(remote);

	if (!disable_fetch_head) {
		/* Write current heads for all desired refs as well as the current (default) ref. */
		const char *current_head = norm_current_head();

		write_initial_fetch_head(remote, current_head);
		for (i = 0; i < refspecs.nr; ++i) {
			write_initial_fetch_head(remote, refspecs.items[i].string);
		}
	}

	for (i = 0; i < remote->url_nr; ++i){
		const char *url = remote->url[i];

		if (!starts_with(url, "http")) {
			warning("journaling is currently only supported for HTTP remotes");
			continue;
		}

		switch (fetch_extents(remote, &r, url)) {
		case HTTP_OK:
			break;
		case HTTP_RANGE_NOT_SATISFIABLE:
			break;
		default:
			++failures;
			error("Failed to fetch extents from '%s'\n",
			      url);
		}

		if (extents_current_state(remote, &r, verbose) < 0)
			die_errno("Failed to load extents data");

		if (fetch_and_process_journals(remote, url, &r) != 0)
			++failures;
	}
	http_cleanup();
	remote_state_store(remote, &r, 1);

	return failures > 0;
}

static int journal_file_exists(struct remote *remote, const char *file)
{
	const char *path = journal_path(remote, "%s", file);
	struct stat st;

	return !stat(path, &st);
}

static void journal_mirror_sanity_check(struct remote *remote)
{
	if (mirror) {
		if (journal_file_exists(remote, "extents.bin")
		    && !journal_file_exists(remote, "metadata.bin"))
			die("you've previously run git journal-fetch without "
			    "--journal-mirror, and now you're trying to run it "
			    "with.  But you've (probably) already applied and "
			    "then deleted some old journals.  Re-downloading "
			    "those journals automatically is not yet "
			    "supported.");
	} else {
		if (journal_file_exists(remote, "metadata.bin"))
			die("metadata.bin exists, but you're running git "
			    "journal-fetch without --journal-mirror.  This is "
			    "likely to lead to corruption.");
	}
}

int main(int argc, const char **argv)
{
	const char *remote = NULL;
	int ret, i = 0;
	struct remote *upstream;

	struct option opts[] = {
		OPT_GROUP(""),
		OPT_BOOL(0, "no-extract", &no_extract,
				"fetch journal content, but do not extract it"),
		OPT_BOOL(0, "journal-mirror", &mirror,
				"mirror mode: update metadata.bin and connectivity data; don't delete replayed journals"),
		OPT_STRING(0, "remote", &remote, "remote",
				"fetch from the remote named <remote_name>"),
		OPT_BOOL(0, "progress", &show_progress,
				"display progress [default: only on interactive sessions]"),
		OPT_BOOL(0, "no-progress", &no_progress,
			 "inhibit display of progress"),
		OPT_BOOL(0, "disable-fetch-head", &disable_fetch_head,
				"disable writing of FETCH_HEAD for extracted tips"),
		OPT_BOOL('q', "quiet", &quiet,
				"minimal output"),
		OPT_BOOL('v', "verbose", &verbose,
				"increase diagnostic output"),
		OPT_BOOL(0, "no-detach", &no_detach,
				"do not detach when processing epilogue actions"),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	argc = parse_options(argc, argv, NULL, opts, journal_fetch_usage, 0);
	git_config(git_default_config, NULL);

	if (argc > 1) {
		remote = argv[i++];
	}
	for (; i < argc; ++i) {
		string_list_append(&refspecs, argv[i]);
	}
	if (!show_progress)
		show_progress = isatty(2);
	if (quiet) {
		show_progress = 0;
		no_progress = 1;
	}
	if (no_progress)
		show_progress = 0;
	if (!remote)
		remote = "origin";
	remote_name_sanity_check(remote);
	upstream = remote_get(remote);
	journal_mirror_sanity_check(upstream);
	if (upstream->mirror) {
		no_detach = 1;
	}
	hashmap_init(&fetch_head_table, NULL, 256);
	ret = journal_fetch(upstream);
	if (!disable_fetch_head) {
		write_fetch_head_file(&fetch_head_table);
	}

	hashmap_free(&fetch_head_table, 1);
	string_list_clear(&refspecs, 0);

	epilogue_perform(ret);
	return ret;
}

