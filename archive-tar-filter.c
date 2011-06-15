#include "cache.h"
#include "archive.h"

struct tar_filter *tar_filters;
static struct tar_filter **tar_filters_tail = &tar_filters;

static struct tar_filter *tar_filter_new(const char *name, int namelen)
{
	struct tar_filter *tf;
	tf = xcalloc(1, sizeof(*tf));
	tf->name = xmemdupz(name, namelen);
	tf->extensions.strdup_strings = 1;
	*tar_filters_tail = tf;
	tar_filters_tail = &tf->next;
	return tf;
}

static void tar_filter_free(struct tar_filter *tf)
{
	string_list_clear(&tf->extensions, 0);
	free(tf->name);
	free(tf->command);
	free(tf);
}

static struct tar_filter *tar_filter_by_namelen(const char *name,
						int len)
{
	struct tar_filter *p;
	for (p = tar_filters; p; p = p->next)
		if (!strncmp(p->name, name, len) && !p->name[len])
			return p;
	return NULL;
}

struct tar_filter *tar_filter_by_name(const char *name)
{
	return tar_filter_by_namelen(name, strlen(name));
}

static int tar_filter_config(const char *var, const char *value, void *data)
{
	struct tar_filter *tf;
	const char *dot;
	const char *name;
	const char *type;
	int namelen;

	if (prefixcmp(var, "tarfilter."))
		return 0;
	dot = strrchr(var, '.');
	if (dot == var + 9)
		return 0;

	name = var + 10;
	namelen = dot - name;
	type = dot + 1;

	tf = tar_filter_by_namelen(name, namelen);
	if (!tf)
		tf = tar_filter_new(name, namelen);

	if (!strcmp(type, "command")) {
		if (!value)
			return config_error_nonbool(var);
		tf->command = xstrdup(value);
		return 0;
	}
	else if (!strcmp(type, "extension")) {
		if (!value)
			return config_error_nonbool(var);
		string_list_append(&tf->extensions, value);
		return 0;
	}
	else if (!strcmp(type, "compressionlevels")) {
		tf->use_compression = git_config_bool(var, value);
		return 0;
	}

	return 0;
}

static void remove_filters_without_command(void)
{
	struct tar_filter *p = tar_filters;
	struct tar_filter **last = &tar_filters;

	while (p) {
		if (p->command && *p->command)
			last = &p->next;
		else {
			*last = p->next;
			tar_filter_free(p);
		}
		p = *last;
	}
}

/*
 * We don't want to load twice, since some of our
 * values actually append rather than overwrite.
 */
static int tar_filter_config_loaded;
extern void tar_filter_load_config(void)
{
	if (tar_filter_config_loaded)
		return;
	tar_filter_config_loaded = 1;

	git_config(tar_filter_config, NULL);
	remove_filters_without_command();
}
