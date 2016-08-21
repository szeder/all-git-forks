#include "cache.h"
#include "parse-options.h"
#include "pkt-line.h"

static char const * const prime_clone_usage[] = {
	N_("git prime-clone [--strict] <dir>"),
	NULL
};

static unsigned int enabled = 1;
static const char *url = NULL, *filetype = NULL;
static int strict;

static struct option prime_clone_options[] = {
	OPT_BOOL(0, "strict", &strict, N_("Do not attempt <dir>/.git if <dir> "
					  "is not a git directory")),
	OPT_END(),
};

static void prime_clone(void)
{
	if (!enabled) {
		fprintf(stderr, _("prime-clone not enabled\n"));
	}
	else if (url && filetype){
		packet_write(1, "%s %s\n", filetype, url);
	}
	else if (url || filetype) {
		if (filetype)
			fprintf(stderr, _("prime-clone not properly "
					  "configured: missing url\n"));
		else if (url)
			fprintf(stderr, _("prime-clone not properly "
					  "configured: missing filetype\n"));
	}
	packet_flush(1);
}

static int prime_clone_config(const char *var, const char *value, void *unused)
{
	if (!strcmp("primeclone.url",var)) {
		return git_config_pathname(&url, var, value);
	}
	if (!strcmp("primeclone.enabled",var)) {
		enabled = git_config_bool(var, value);
		return 0;
	}
	if (!strcmp("primeclone.filetype",var)) {
		return git_config_string(&filetype, var, value);
	}
	return git_default_config(var, value, unused);
}

int cmd_prime_clone(int argc, const char **argv, const char *prefix)
{
	const char *dir;
	argc = parse_options(argc, argv, prefix, prime_clone_options,
			     prime_clone_usage, 0);
	if (argc == 0) {
		usage_msg_opt(_("No repository specified."), prime_clone_usage,
			      prime_clone_options);
	}
	else if (argc > 1) {
		usage_msg_opt(_("Too many arguments."), prime_clone_usage,
			      prime_clone_options);
	}

	dir = argv[0];

	if (!enter_repo(dir, 0)){
		die(_("'%s' does not appear to be a git repository"), dir);
	}

	git_config(prime_clone_config, NULL);
	prime_clone();
	return 0;
}
