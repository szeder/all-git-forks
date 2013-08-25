#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char *argv[])
{

	unsigned int output=1, i;
	FILE *f=NULL;
	char file[4096];
	do {
		if (f) {
			fclose(f);
			output++;
		}
		snprintf(file, sizeof(file), "commandline%u", output);
		f = fopen(file, "r");
	} while (f);

	snprintf(file, sizeof(file), "commandline%u", output);
	f = fopen(file, "w");
	if (!f)
		exit(1);

	for (i = 2; i < argc; i++) {
		fprintf(f, "!%s!\n", argv[i]);
	}
	fclose(f);

	snprintf(file, sizeof(file), "msgtxt%u", output);
	f = fopen(file, "w");
	if (!f)
		exit(1);

	while (!feof(stdin)) {
		char buffer[4096];
		size_t count = fread(buffer, 1, sizeof(buffer), stdin);
		fwrite(buffer, 1, count, f);
	}
	fclose(f);

}
