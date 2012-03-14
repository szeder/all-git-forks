#include "cache.h"
#include "run-command.h"
#include "sigchain.h"
#include "connected.h"

/*
 * If we feed all the commits we want to verify to this command
 *
 *  $ git rev-list --verify-objects --stdin --not --all
 *
 * and if it does not error out, that means everything reachable from
 * these commits locally exists and is connected to some of our
 * existing refs.
 *
 * Returns 0 if everything is connected, non-zero otherwise.
 */
int check_everything_connected(sha1_iterate_fn fn, unsigned int flags,
			       const char *pack_lockfile, void *cb_data)
{
	struct child_process rev_list;
	const char *argv[] = {"rev-list", "--verify-objects", "--stdin",
			      "--not", "--all", NULL, NULL, NULL, NULL };
	char commit[41];
	unsigned char sha1[20];
	int err = 0, ac = 5;
	struct strbuf packfile = STRBUF_INIT;

	if (fn(cb_data, sha1))
		return err;

	if (flags & CHECK_CONNECT_QUIET)
		argv[ac++] = "--quiet";
	if (pack_lockfile) {
		strbuf_addstr(&packfile, pack_lockfile);
		/* xxx/pack-%40s.keep */
		assert(strcmp(packfile.buf + packfile.len - 5, ".keep") == 0);
		assert(strncmp(packfile.buf + packfile.len - 51, "/pack-", 6) == 0);
		strbuf_setlen(&packfile, packfile.len - 5);
		strbuf_remove(&packfile, 0, packfile.len - 40);
		argv[ac++] = "--safe-pack";
		argv[ac++] = packfile.buf;
	}
	assert(ac < ARRAY_SIZE(argv) && argv[ac] == NULL);

	memset(&rev_list, 0, sizeof(rev_list));
	rev_list.argv = argv;
	rev_list.git_cmd = 1;
	rev_list.in = -1;
	rev_list.no_stdout = 1;
	rev_list.no_stderr = flags & CHECK_CONNECT_QUIET;
	err = start_command(&rev_list);
	strbuf_release(&packfile);
	if (err)
		return error(_("Could not run 'git rev-list'"));

	sigchain_push(SIGPIPE, SIG_IGN);

	commit[40] = '\n';
	do {
		memcpy(commit, sha1_to_hex(sha1), 40);
		if (write_in_full(rev_list.in, commit, 41) < 0) {
			if (errno != EPIPE && errno != EINVAL)
				error(_("failed write to rev-list: %s"),
				      strerror(errno));
			err = -1;
			break;
		}
	} while (!fn(cb_data, sha1));

	if (close(rev_list.in)) {
		error(_("failed to close rev-list's stdin: %s"), strerror(errno));
		err = -1;
	}

	sigchain_pop(SIGPIPE);
	return finish_command(&rev_list) || err;
}
