#include "cache.h"
#include "refs.h"

#define PRIME_CLONE_ENABLED 1

static const char prime_clone_usage[] = "git prime-clone [--strict] <dir>";

static unsigned int enabled;
static const char *url = "\0";
static const char *filetype = "\0";

static void prime_clone(void)
{
	if (enabled)
	{
		if (strlen(url) != 0 && strlen(filetype) != 0) {
			packet_write(1, "url %s\n", url);
			packet_write(1, "filetype %s\n", filetype);
		}
		else {
			packet_write(1, "prime-clone not properly configured\n");
		}
	}
	else {
		packet_write(1, "prime-clone not enabled\n");
	}
	packet_flush(1);
}

static int prime_clone_config(const char *var, const char *value, void *unused)
{
	if (!strcmp("primeclone.url",var)) {
		return git_config_pathname(&url, var, value);
	}
	if (!strcmp("primeclone.enabled",var)) {
		if (git_config_bool(var, value))
			enabled = PRIME_CLONE_ENABLED;
		else
			enabled = ~PRIME_CLONE_ENABLED;
	}
	if (!strcmp("primeclone.filetype",var)) {
		return git_config_string(&filetype, var, value);
	}
	return parse_hide_refs_config(var, value, "primeclone");
}

int main(int argc, char **argv)
{
	char *dir;
	int i;
	int strict = 0;

	git_setup_gettext();

	packet_trace_identity("prime-clone");
	git_extract_argv0_path(argv[0]);
	check_replace_refs = 0;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (arg[0] != '-')
			break;
		if (!strcmp(arg, "--strict")) {
			strict = 1;
			continue;
		}
		if (!strcmp(arg, "--")) {
			i++;
			break;
		}
	}

	if (i != argc-1)
		usage(prime_clone_usage);

	setup_path();

	dir = argv[i];

	if (!enter_repo(dir, strict))
		die("'%s' does not appear to be a git repository", dir);

	git_config(prime_clone_config, NULL);
	prime_clone();
	return 0;
}
