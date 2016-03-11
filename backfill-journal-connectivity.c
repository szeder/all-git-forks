#include "cache.h"
#include "journal-connectivity.h"

int main(int argc, char **argv) {
	setup_git_directory();

	jcdb_backfill();

	return 0;
}
