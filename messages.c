#include "cache.h"
#include "messages.h"

struct message_preference messages[] = {
	{ "pushnonfastforward", 1 },
	{ "statusadvice", 1 },
};

int git_default_message_config(const char *var, const char *value)
{
	const char *k = skip_prefix(var, "message.");
	int i;

	if (!strcmp(k, "all")) {
		int v = git_config_bool(var, value);
		for (i = 0; i < ARRAY_SIZE(messages); i++)
			messages[i].preference = v;
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(messages); i++) {
		if (strcmp(k, messages[i].name))
			continue;
		messages[i].preference = git_config_bool(var, value);
		return 0;
	}

	return 0;
}
