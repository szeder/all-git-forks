#include "cache.h"
#include "parse-options.h"
#include "sigchain.h"
#include "strbuf.h"
#include "exec_cmd.h"
#include "split-index.h"
#include "shm.h"
#include "lockfile.h"
#include "watchman-support.h"

struct shm {
	unsigned char sha1[20];
	void *shm;
	size_t size;
	pid_t pid;
};

static struct shm shm_index;
static struct shm shm_base_index;
static struct shm shm_watchman;
static int daemonized, to_verify = 1;

static void release_index_shm(struct shm *is)
{
	if (!is->shm)
		return;
	munmap(is->shm, is->size);
	git_shm_unlink("git-index-%s", sha1_to_hex(is->sha1));
	is->shm = NULL;
}

static void release_watchman_shm(struct shm *is)
{
	if (!is->shm)
		return;
	munmap(is->shm, is->size);
	git_shm_unlink("git-watchman-%s-%" PRIuMAX,
		       sha1_to_hex(is->sha1), (uintmax_t)is->pid);
	is->shm = NULL;
}

static void cleanup_shm(void)
{
	release_index_shm(&shm_index);
	release_index_shm(&shm_base_index);
	release_watchman_shm(&shm_watchman);
}

static void cleanup(void)
{
	if (daemonized)
		return;
	unlink(git_path("index-helper.pipe"));
	cleanup_shm();
}

static void cleanup_on_signal(int sig)
{
	/* We ignore sigpipes -- that's just a client being broken. */
	if (sig == SIGPIPE)
		return;
	cleanup();
	sigchain_pop(sig);
	raise(sig);
}

static void share_index(struct index_state *istate, struct shm *is)
{
	void *new_mmap;
	if (istate->mmap_size <= 20 ||
	    hashcmp(istate->sha1,
		    (unsigned char *)istate->mmap + istate->mmap_size - 20) ||
	    !hashcmp(istate->sha1, is->sha1) ||
	    git_shm_map(O_CREAT | O_EXCL | O_RDWR, 0700, istate->mmap_size,
			&new_mmap, PROT_READ | PROT_WRITE, MAP_SHARED,
			"git-index-%s", sha1_to_hex(istate->sha1)) < 0)
		return;

	release_index_shm(is);
	is->size = istate->mmap_size;
	is->shm = new_mmap;
	hashcpy(is->sha1, istate->sha1);
	memcpy(new_mmap, istate->mmap, istate->mmap_size - 20);

	/*
	 * The trailing hash must be written last after everything is
	 * written. It's the indication that the shared memory is now
	 * ready.
	 */
	hashcpy((unsigned char *)new_mmap + istate->mmap_size - 20, is->sha1);
}

static int verify_shm(void)
{
	int i;
	struct index_state istate;
	memset(&istate, 0, sizeof(istate));
	istate.always_verify_trailing_sha1 = 1;
	istate.to_shm = 1;
	i = read_index(&istate);
	if (i != the_index.cache_nr)
		goto done;
	for (; i < the_index.cache_nr; i++) {
		struct cache_entry *base, *ce;
		/* namelen is checked separately */
		const unsigned int ondisk_flags =
			CE_STAGEMASK | CE_VALID | CE_EXTENDED_FLAGS;
		unsigned int ce_flags, base_flags, ret;
		base = the_index.cache[i];
		ce = istate.cache[i];
		if (ce->ce_namelen != base->ce_namelen ||
		    strcmp(ce->name, base->name)) {
			warning("mismatch at entry %d", i);
			break;
		}
		ce_flags = ce->ce_flags;
		base_flags = base->ce_flags;
		/* only on-disk flags matter */
		ce->ce_flags   &= ondisk_flags;
		base->ce_flags &= ondisk_flags;
		ret = memcmp(&ce->ce_stat_data, &base->ce_stat_data,
			     offsetof(struct cache_entry, name) -
			     offsetof(struct cache_entry, ce_stat_data));
		ce->ce_flags = ce_flags;
		base->ce_flags = base_flags;
		if (ret) {
			warning("mismatch at entry %d", i);
			break;
		}
	}
done:
	discard_index(&istate);
	return i == the_index.cache_nr;
}

static void share_the_index(void)
{
	if (the_index.split_index && the_index.split_index->base)
		share_index(the_index.split_index->base, &shm_base_index);
	share_index(&the_index, &shm_index);
	if (to_verify && !verify_shm()) {
		cleanup_shm();
		discard_index(&the_index);
	}
}

#ifdef HAVE_SHM

#ifdef USE_WATCHMAN
static void share_watchman(struct index_state *istate,
			   struct shm *is, pid_t pid)
{
	struct strbuf sb = STRBUF_INIT;
	void *shm;

	write_watchman_ext(&sb, istate);
	if (git_shm_map(O_CREAT | O_EXCL | O_RDWR, 0700, sb.len + 20,
			&shm, PROT_READ | PROT_WRITE, MAP_SHARED,
			"git-watchman-%s-%" PRIuMAX,
			sha1_to_hex(istate->sha1), (uintmax_t)pid) == sb.len + 20) {
		is->size = sb.len + 20;
		is->shm = shm;
		is->pid = pid;
		hashcpy(is->sha1, istate->sha1);

		memcpy(shm, sb.buf, sb.len);
		hashcpy((unsigned char *)shm + is->size - 20, is->sha1);
	}
	strbuf_release(&sb);
}


static void prepare_with_watchman(pid_t pid)
{
	/*
	 * TODO: with the help of watchman, maybe we could detect if
	 * $GIT_DIR/index is updated.
	 */
	if (check_watchman(&the_index))
		return;

	share_watchman(&the_index, &shm_watchman, pid);
}

static void prepare_index(pid_t pid)
{
	release_watchman_shm(&shm_watchman);
	if (the_index.last_update)
		prepare_with_watchman(pid);
}

#endif

static void refresh(void)
{
	discard_index(&the_index);
	the_index.keep_mmap = 1;
	the_index.to_shm    = 1;
	if (read_cache() < 0)
		die(_("could not read index"));
	share_the_index();
}

static int send_response(const char *client_pipe_path, const char *response)
{
	int fd;
	int len;

	fd = open(client_pipe_path, O_WRONLY | O_NONBLOCK);
	if (fd < 0)
		return -1;

	len = strlen(response) + 1;
	assert(len < PIPE_BUF);
	if (write_in_full(fd, response, len) != len) {
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

#ifdef USE_WATCHMAN
static uintmax_t get_trailing_digits(const char *path)
{
	const char *start = strrchr(path, '/');
	if (!start)
		return 0;
	while (*start && !isdigit(*start)) start ++;
	if (!*start)
		return 0;
	return strtoull(start, NULL, 10);
}
#endif

static void reply(const char *path)
{
#ifdef USE_WATCHMAN
	uintmax_t pid;
	/*
	 * Parse caller pid out of provided path.  It'll be some
	 * digits on the end.
	 */
	pid = (pid_t)get_trailing_digits(path);
	prepare_index(pid);
#endif
	send_response(path, "OK");
}

static void loop(int fd, int idle_in_seconds)
{
	struct timeval timeout;
	struct timeval *timeout_p;

	if (idle_in_seconds) {
		timeout.tv_usec = 0;
		timeout.tv_sec = idle_in_seconds;
		timeout_p = &timeout;
	} else {
		timeout_p = NULL;
	}

	while (1) {
		fd_set readfds;

		/* Wait for a request */
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		if (select(fd + 1, &readfds, NULL, NULL, timeout_p)) {
			/* Got one! */
			struct strbuf command = STRBUF_INIT;
			/*
			 * We assume that after the client does a
			 * write, we can do a full read of the data
			 * they wrote (which will be less than
			 * PIPE_BUF).
			 */
			if (strbuf_getwholeline_fd(&command, fd, '\0') == 0) {
				if (!strcmp(command.buf, "refresh")) {
					refresh();
				} else if (starts_with(command.buf, "poke")) {
					if (command.buf[4] == ' ')
						reply(command.buf + 5);
					else
						/*
						 * Just a poke to keep us
						 * alive, nothing to do.
						 */
						;
				} else if (!strcmp(command.buf, "die")) {
					break;
				} else {
					warning("BUG: Bogus command %s", command.buf);
				}
			}
		} else {
			/* No request before timeout */
			break;
		}
	}

	close(fd);
}

#else

static void loop(int fd, int idle_in_seconds)
{
	die(_("index-helper is not supported on this platform"));
}

#endif

static int setup_pipe(const char *pipe_path)
{
	int fd;

	if (mkfifo(pipe_path, 0600)) {
		if (errno != EEXIST)
			die(_("failed to create pipe %s"), pipe_path);

		/* Left over from a previous run, delete & retry */
		if (unlink(pipe_path))
			die(_("failed to delete %s"), pipe_path);
		if (mkfifo(pipe_path, 0600))
			die(_("failed to create pipe %s"), pipe_path);
	}

	/*
	 * Even though we never write to this pipe, we need to open
	 * O_RDWR to prevent select() looping on EOF.
	 */
	fd = open(pipe_path, O_RDWR | O_NONBLOCK);
	if (fd < 0)
		die(_("Failed to open %s"), pipe_path);
	return fd;
}

static const char * const usage_text[] = {
	N_("git index-helper [options]"),
	NULL
};

static void request_kill(const char *pipe_path)
{
	int fd;

	fd = open(pipe_path, O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		warning("No existing pipe; can't send kill message to old process");
		goto done;
	}

	write_in_full(fd, "die", 4);
	close(fd);

done:
	/*
	 * The child will try to do this anyway, but we want to be
	 * ready to launch a new daemon immediately after this command
	 * returns.
	 */

	unlink(pipe_path);
	return;
}


int main(int argc, char **argv)
{
	const char *prefix;
	int idle_in_seconds = 600, detach = 0, kill = 0, autorun = 0;
	int fd;
	int nongit;
	struct strbuf pipe_path = STRBUF_INIT;
	struct option options[] = {
		OPT_INTEGER(0, "exit-after", &idle_in_seconds,
			    N_("exit if not used after some seconds")),
		OPT_BOOL(0, "strict", &to_verify,
			 "verify shared memory after creating"),
		OPT_BOOL(0, "detach", &detach, "detach the process"),
		OPT_BOOL(0, "kill", &kill, "request that existing index helper processes exit"),
		OPT_BOOL(0, "autorun", &autorun, "this is an automatic run of git index-helper, so certain errors can be solved by silently exiting"),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);
	git_setup_gettext();

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage_with_options(usage_text, options);

	prefix = setup_git_directory_gently(&nongit);
	if (nongit) {
		if (autorun)
			exit(0);
		else
			die("Not a git repository");
	}

	if (parse_options(argc, (const char **)argv, prefix,
			  options, usage_text, 0))
		die(_("too many arguments"));

	strbuf_git_path(&pipe_path, "index-helper.pipe");
	if (kill) {
		if (detach)
			die(_("--kill doesn't want any other options"));
		request_kill(pipe_path.buf);
		return 0;
	}

	/* check that no other copy is running */
	fd = open(pipe_path.buf, O_RDONLY | O_NONBLOCK);
	if (fd > 0) {
		if (autorun)
			return 0;
		else
			die(_("Already running"));
	}
	if (errno != ENXIO && errno != ENOENT) {
		if (autorun)
			return 0;
		else
			die_errno(_("Unexpected error checking pipe"));
	}

	atexit(cleanup);
	sigchain_push_common(cleanup_on_signal);

	fd = setup_pipe(pipe_path.buf);
	if (fd < 0)
		die_errno("Could not set up index-helper.pipe");

	if (detach && daemonize(&daemonized))
		die_errno("unable to detach");

	loop(fd, idle_in_seconds);
	return 0;
}
