#include "cache.h"
#include "parse-options.h"
#include "sigchain.h"
#include "strbuf.h"
#include "exec_cmd.h"
#include "split-index.h"
#include "shm.h"
#include "lockfile.h"
#include "cache.h"
#include "unix-socket.h"

struct shm {
	unsigned char sha1[20];
	void *shm;
	size_t size;
};

static struct shm shm_index;
static struct shm shm_base_index;
static int to_verify = 1;

static void release_index_shm(struct shm *is)
{
	if (!is->shm)
		return;
	munmap(is->shm, is->size);
	git_shm_unlink("git-idx-%s", sha1_to_shm_path(is->sha1));
	is->shm = NULL;
}

static void cleanup_shm(void)
{
	release_index_shm(&shm_index);
	release_index_shm(&shm_base_index);
}

static void cleanup(void)
{
	unlink(git_path("index-helper.path"));
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
			"git-idx-%s", sha1_to_shm_path(istate->sha1)) < 0)
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
	 * The memory barrier here matches read-cache.c:try_shm.
	 */
	__sync_synchronize();

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
	if (to_verify && !verify_shm())
		cleanup_shm();
	discard_index(&the_index);
}

static void refresh(void)
{
	the_index.keep_mmap = 1;
	the_index.to_shm    = 1;
	if (read_cache() < 0)
		die(_("could not read index"));
	share_the_index();
}

static void set_socket_blocking_flag(int fd, int make_nonblocking)
{
	int flags;

	flags = fcntl(fd, F_GETFL, NULL);

	if (flags < 0)
		die(_("fcntl failed"));

	if (make_nonblocking)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if (fcntl(fd, F_SETFL, flags) < 0)
		die(_("fcntl failed"));
}

#ifdef HAVE_SHM

static void loop(int fd, int idle_in_seconds)
{
	struct timeval timeout;
	struct timeval *timeout_p;

	while (1) {
		fd_set readfds;
		int result, client_fd;
		struct strbuf command = STRBUF_INIT;

		/* need to reset timer in case select() decremented it */
		if (idle_in_seconds) {
			timeout.tv_usec = 0;
			timeout.tv_sec = idle_in_seconds;
			timeout_p = &timeout;
		} else {
			timeout_p = NULL;
		}

		/* Wait for a request */
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		result = select(fd + 1, &readfds, NULL, NULL, timeout_p);
		if (result < 0)
			die_errno(_("select() failed"));
		if (result == 0)
			/* timeout */
			break;

		client_fd = accept(fd, NULL, NULL);
		if (client_fd < 0)
			/*
			 * An error here is unlikely -- it probably
			 * indicates that the connecting process has
			 * already dropped the connection.
			 */
			continue;

		/*
		 * Our connection to the client is blocking since a client
		 * can always be killed by SIGINT or similar.
		 */
		set_socket_blocking_flag(client_fd, 0);

		if (strbuf_getwholeline_fd(&command, client_fd, '\0') == 0) {
			if (!strcmp(command.buf, "refresh")) {
				refresh();
			} else if (!strcmp(command.buf, "poke")) {
				/*
				 * Just a poke to keep us
				 * alive, nothing to do.
				 */
			} else {
				warning("BUG: Bogus command %s", command.buf);
			}
		} else {
			/*
			 * No command from client.  Probably it's just
			 * a liveness check.  Just close up.
			 */
		}
		close(client_fd);
		strbuf_release(&command);
	}

	close(fd);
}

#else

static void loop(int fd, int idle_in_seconds)
{
	die(_("index-helper is not supported on this platform"));
}

#endif

static const char * const usage_text[] = {
	N_("git index-helper [options]"),
	NULL
};

static void make_socket_path(struct strbuf *path)
{
	const char *tmpdir;

	tmpdir = getenv("TMPDIR");
	if (!tmpdir)
		tmpdir = "/tmp";

	/*
	 * We need to make a dir that we can own, so that users other
	 * than us cannot poke the daemon, and so that, if the daemon
	 * dies, no other process can recreate the socket and trick us
	 * into talking to an imposter.
	 */
	strbuf_addf(path, "%s/XXXXXX", tmpdir);
	if (!mkdtemp(path->buf))
		die(("failed to make temp dir for socket"));

	/*
	 * Use a stupid filename because we want to minimize the path
	 * length since socket filenames must be short.
	 */
	strbuf_addstr(path, "/s");
}

int main(int argc, char **argv)
{
	const char *prefix;
	int idle_in_seconds = 600;
	int fd;
	struct strbuf socket_path = STRBUF_INIT;
	struct option options[] = {
		OPT_INTEGER(0, "exit-after", &idle_in_seconds,
			    N_("exit if not used after some seconds")),
		OPT_BOOL(0, "strict", &to_verify,
			 "verify shared memory after creating"),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);
	git_setup_gettext();

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage_with_options(usage_text, options);

	prefix = setup_git_directory();
	if (parse_options(argc, (const char **)argv, prefix,
			  options, usage_text, 0))
		die(_("too many arguments"));

	atexit(cleanup);
	sigchain_push_common(cleanup_on_signal);

	make_socket_path(&socket_path);
	fd = unix_stream_listen(socket_path.buf);
	if (fd < 0)
		die_errno(_("could not set up index-helper socket"));
	/*
	 * Now that the socket is set up, we symlink it into
	 * GIT_DIR so clients can find it.
	 */
	if (unlink(git_path("index-helper.path")) && errno != ENOENT)
		die(_("failed to delete old index-helper.path"));
	if (symlink(socket_path.buf, git_path("index-helper.path")))
		die(_("failed to symlink socket path into index-helper.path"));
	loop(fd, idle_in_seconds);

	return 0;
}
