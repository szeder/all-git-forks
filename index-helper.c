#include "cache.h"
#include "parse-options.h"
#include "sigchain.h"
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

static void free_index_shm(struct shm *is)
{
	if (!is->shm)
		return;
	munmap(is->shm, is->size);
	git_shm_unlink("git-index-%s", sha1_to_hex(is->sha1));
	is->shm = NULL;
}

static void free_watchman_shm(struct shm *is)
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
	free_index_shm(&shm_index);
	free_index_shm(&shm_base_index);
	free_watchman_shm(&shm_watchman);
}

static void cleanup(void)
{
	if (daemonized)
		return;
	unlink(git_path("index-helper.pid"));
	cleanup_shm();
}

static void cleanup_on_signal(int sig)
{
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

	free_index_shm(is);
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

static void refresh(int sig)
{
	discard_index(&the_index);
	the_index.keep_mmap = 1;
	the_index.to_shm    = 1;
	if (read_cache() < 0)
		die(_("could not read index"));
	share_the_index();
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
	 * with the help of watchman, maybe we could detect if
	 * $GIT_DIR/index is updated..
	 */
	if (!verify_index(&the_index))
		refresh(0);

	if (check_watchman(&the_index))
		return;

	share_watchman(&the_index, &shm_watchman, pid);
}

static void prepare_index(int sig, siginfo_t *si, void *context)
{
	free_watchman_shm(&shm_watchman);
	if (the_index.last_update)
		prepare_with_watchman(si->si_pid);
	kill(si->si_pid, SIGHUP); /* stop the waiting in poke_daemon() */
}

#else

static void prepare_index(int sig, siginfo_t *si, void *context)
{
	/*
	 * what we need is the signal received and interrupts
	 * sleep(). We don't need to do anything else when receving
	 * the signal
	 */
}

#endif

static void loop(const char *pid_file, int idle_in_seconds)
{
	struct sigaction sa;

	sigchain_pop(SIGHUP);	/* pushed by sigchain_push_common */
	sigchain_push(SIGHUP, refresh);

	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = prepare_index;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGUSR1, &sa, NULL);

	refresh(0);
	while (sleep(idle_in_seconds))
		; /* do nothing, all is handled by signal handlers already */
}

#elif defined(GIT_WINDOWS_NATIVE)

static void loop(const char *pid_file, int idle_in_seconds)
{
	HWND hwnd;
	UINT_PTR timer = 0;
	MSG msg;
	HINSTANCE hinst = GetModuleHandle(NULL);
	WNDCLASS wc;

	/*
	 * Emulate UNIX signals by sending WM_USER+x to a
	 * window. Register window class and create a new window to
	 * catch these messages.
	 */
	memset(&wc, 0, sizeof(wc));
	wc.lpfnWndProc	 = DefWindowProc;
	wc.hInstance	 = hinst;
	wc.lpszClassName = "git-index-helper";
	if (!RegisterClass(&wc))
		die_errno(_("could not register new window class"));

	hwnd = CreateWindow("git-index-helper", pid_file,
			    0, 0, 0, 1, 1, NULL, NULL, hinst, NULL);
	if (!hwnd)
		die_errno(_("could not register new window"));

	refresh(0);
	while (1) {
		timer = SetTimer(hwnd, timer, idle_in_seconds * 1000, NULL);
		if (!timer)
			die(_("no timer!"));
		if (!GetMessage(&msg, hwnd, 0, 0) || msg.message == WM_TIMER)
			break;
		switch (msg.message) {
		case WM_USER:
			refresh(0);
			break;
		default:
			/* just reset the timer */
			break;
		}
	}
}

#else

static void loop(const char *pid_file, int idle_in_seconds)
{
	die(_("index-helper is not supported on this platform"));
}

#endif

static const char * const usage_text[] = {
	N_("git index-helper [options]"),
	NULL
};

int main(int argc, char **argv)
{
	static struct lock_file lock;
	struct strbuf sb = STRBUF_INIT;
	const char *prefix;
	int fd, idle_in_minutes = 10, detach = 0;
	struct option options[] = {
		OPT_INTEGER(0, "exit-after", &idle_in_minutes,
			    N_("exit if not used after some minutes")),
		OPT_BOOL(0, "strict", &to_verify,
			 "verify shared memory after creating"),
		OPT_BOOL(0, "detach", &detach, "detach the process"),
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

	fd = hold_lock_file_for_update(&lock,
				       git_path("index-helper.pid"),
				       LOCK_DIE_ON_ERROR);
#ifdef GIT_WINDOWS_NATIVE
	strbuf_addstr(&sb, "HWND");
#elif defined(USE_WATCHMAN)
	strbuf_addch(&sb, 'W');	/* see poke_daemon() */
#endif
	strbuf_addf(&sb, "%" PRIuMAX, (uintmax_t) getpid());
	write_in_full(fd, sb.buf, sb.len);
	commit_lock_file(&lock);

	atexit(cleanup);
	sigchain_push_common(cleanup_on_signal);

	if (detach && daemonize(&daemonized))
		die_errno("unable to detach");

	if (!idle_in_minutes)
		idle_in_minutes = 0xffffffff / 60;
	loop(sb.buf, idle_in_minutes * 60);
	strbuf_release(&sb);
	return 0;
}
