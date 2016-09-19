#include "cache.h"

static const char usage_str[] = "(write|delete|read) <args>...";
static const char write_usage_str[] = "write <shared-index> <path>";

static void sha1_from_path(unsigned char *sha1, const char *path)
{
	git_SHA_CTX ctx;

	git_SHA1_Init(&ctx);
	git_SHA1_Update(&ctx, path, strlen(path));
	git_SHA1_Final(sha1, &ctx);
}

static void write_s_i_info(const char *shared_index, const char *path)
{
	unsigned char path_sha1[GIT_SHA1_RAWSZ];
	struct strbuf s_i_info = STRBUF_INIT;

	sha1_from_path(path_sha1, path);

	strbuf_git_path(&s_i_info, "sharedindex-info/%s-%s",
			shared_index, sha1_to_hex(path_sha1));

	switch (safe_create_leading_directories(s_i_info.buf))
	{
	case SCLD_OK:
		break; /* success */
	case SCLD_EXISTS:
		die("unable to create directory for '%s' "
		    "as a file with the same name as a directory already exists",
		    s_i_info.buf);
	case SCLD_VANISHED:
		die("unable to create directory for '%s' "
		    "as an underlying directory was just pruned; "
		    "maybe try again?",
		    s_i_info.buf);
	case SCLD_PERMS:
		die("unable to create directory for '%s' "
		    "because of permission problems",
		    s_i_info.buf);
	default:
		die("unable to create directory for '%s'", s_i_info.buf);
	}

	write_file(s_i_info.buf, "%s", path);

	strbuf_release(&s_i_info);
}

static void handle_write_command(int ac, const char **av)
{
	if (ac != 4)
		die("%s\nusage: %s %s",
		    "write command requires exactly 2 arguments",
		    av[0], write_usage_str);

	write_s_i_info(av[2], av[3]);
}

static void show_args(int ac, const char **av)
{
	int i;

	for (i = 0; i < ac; i++) {
		printf("av[%d]: %s\n", i, av[i]);
	}
}

int cmd_main(int ac, const char **av)
{
	const char *command;

	if (ac < 2)
		die("%s\nusage: %s %s", "too few arguments", av[0], usage_str);

	command = av[1];

	if (!strcmp(command, "write"))
		handle_write_command(ac, av);
	else
		show_args(ac, av);

	return 0;
}
