#include "builtin.h"
#include "cache.h"
#include "transport.h"
#include "remote.h"

static const char * const git_update_ref_remote_usage[] = {
  N_("git update-ref-remote [options] <repository> -d <ref> [<oldval>]"),
  N_("git update-ref-remote [options] <repository>    <ref> <newval> [<oldval>]"),
  N_("git update-ref-remote [options] <repository> --stdin [-z]"),
  NULL
};


static int update_ref_remote_one(struct transport *transport, const char* refarg,
				 const char* newval, const char* oldval) {
  if (oldval == NULL)
    abort(); // TODO: support omitting the old value
  struct ref *ref = alloc_ref(refarg);
  get_sha1_hex(newval, &(ref->new_sha1));
  get_sha1_hex(oldval, &(ref->old_sha1));
  int ret = transport_update_ref_remote(transport, ref);
  free_refs(ref);
  return ret;
}

static int update_ref_remote_stdin(struct transport *transport, int end_null) {
  abort(); // TODO
  return 1;
}

int cmd_update_ref_remote(int argc, const char **argv, const char *prefix)
{
	const char *dest, *refarg, *newval, *oldval = NULL;
	const char *uploadpack = NULL;
	int status = 0;

	struct remote *remote;
	struct transport *transport;

	int delete = 0, read_stdin = 0, end_null = 0;
	struct option options[] = {
		OPT_BOOL('d', NULL, &delete, N_("delete the reference")),
		OPT_BOOL('z', NULL, &end_null, N_("stdin has NULL-terminated arguments")),
		OPT_BOOL( 0 , "stdin", &read_stdin, N_("read updates from stdin")),
		OPT_STRING('u', "upload-pack", &uploadpack, N_("git-upload-pack"),
			   N_("command to use as the git-upload-pack command on the remote host")),
		OPT_END(),
	};

	git_config(git_default_config, NULL);
	argc = parse_options(argc, argv, prefix, options, git_update_ref_remote_usage,
			     0);

	if (delete) {
	  // -d usage: <remote> <ref>
	  // also, not allowed with --stdin
	  if (read_stdin || argc < 2 || argc > 3)
	    usage_with_options(git_update_ref_remote_usage, options);
	  dest = argv[0];
	  refarg = argv[1];
	  if (argc > 2)
	    oldval = argv[2];
	} else if (read_stdin) {
	  // --stdin usage: <remote>
	  // also, not allowed with -d
	  if (delete || argc != 1)
	    usage_with_options(git_update_ref_remote_usage, options);
	  dest = argv[0];
	} else {
	  // the default usage is <remote> <ref> <newval> [<oldval>]
	  if (argc < 3 || argc > 4)
	    usage_with_options(git_update_ref_remote_usage, options);
	  dest = argv[0];
	  refarg = argv[1];
	  newval = argv[2];
	  if (argc > 3)
	    oldval = argv[3];
	}

	remote = remote_get(dest);
	if (!remote) {
		if (dest)
			die("bad repository '%s'", dest);
		die("No remote configured to update.");
	}
	if (!remote->url_nr)
		die("remote %s has no configured URL", dest);

	transport = transport_get(remote, NULL);
	if (uploadpack != NULL)
		transport_set_option(transport, TRANS_OPT_UPLOADPACK, uploadpack);

	if (read_stdin) {
	  update_ref_remote_stdin(transport, end_null);
	} else {
	  update_ref_remote_one(transport, refarg, newval, oldval);
	}

	return status;
}
