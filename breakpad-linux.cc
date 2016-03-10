#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "client/linux/handler/exception_handler.h"

extern "C" {
extern void finish_stats_report(int status);
}


static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
			 void* context,
			 bool succeeded)
{
	fprintf(stderr, "Git crashed; dumped state to %s\n", descriptor.path());
	finish_stats_report(-1);
	return succeeded;
}

static google_breakpad::MinidumpDescriptor *descriptor;
static google_breakpad::ExceptionHandler *exception_handler;

extern "C" {

void init_minidump(const char* path)
{
	mkdir(path, 02775);
	chmod(path, 02775);
	descriptor = new google_breakpad::MinidumpDescriptor(path);
	exception_handler = new google_breakpad::ExceptionHandler(*descriptor,
								  0,
								  dumpCallback,
								  0,
								  true,
								  -1);

}

}
