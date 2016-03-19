#include "cache.h"
#include "parse-options.h"
#include "sigchain.h"
#include "strbuf.h"
#include "exec_cmd.h"
#include "split-index.h"
#include "shm.h"
#include "lockfile.h"

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
	git_shm_unlink("git-index-%s", sha1_to_hex(is->sha1));
	is->shm = NULL;
}

static void cleanup_shm(void)
{
	release_index_shm(&shm_index);
	release_index_shm(&shm_base_index);
}

static void cleanup(void)
{
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

#ifdef HAVE_SHM

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
				} else if (!strcmp(command.buf, "poke")) {
					  /*
					   * Just a poke to keep us
					   * alive, nothing to do.
					   */
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

int main(int argc, char **argv)
{
	const char *prefix;
	int idle_in_seconds = 600;
	int fd;
	struct strbuf pipe_path = STRBUF_INIT;
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

	strbuf_git_path(&pipe_path, "index-helper.pipe");
	fd = setup_pipe(pipe_path.buf);
	if (fd < 0)
		die_errno("Could not set up index-helper.pipe");
	loop(fd, idle_in_seconds);
	return 0;
}
