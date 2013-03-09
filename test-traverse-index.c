#include "git-compat-util.h"

static const char *usage_msg = "test-traverse-index";

int main(int argc, char **argv)
{
	if (argc > 1)
		usage(usage_msg);

	return 0;
}
