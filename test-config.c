#include "cache.h"

static const char *config_string = "[some]\n"
			    "	value = content\n";

static int config_strbuf(const char *var, const char *value, void *data)
{
	int *success = data;
	if (!strcmp(var, "some.value") && !strcmp(value, "content"))
		*success = 0;

	printf("var: %s, value: %s\n", var, value);

	return 1;
}

int main(int argc, char **argv)
{
	int success = 1;
	struct strbuf buf = STRBUF_INIT;

	strbuf_addstr(&buf, config_string);
	git_config_from_strbuf(config_strbuf, &buf, &success);

	return success;
}
