#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include "client/mac/handler/exception_handler.h"

static google_breakpad::ExceptionHandler *exception_handler = 0;

extern "C" {
	extern void finish_stats_report(int status);

	// Cribbed from Breakpad.mm
	static bool isInDebugger()
	{
		bool result = false;

		pid_t pid = getpid();
		int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
		int mibSize = sizeof(mib) / sizeof(int);
		size_t actualSize;
		if (sysctl(mib, mibSize, NULL, &actualSize, NULL, 0) == 0) {
			struct kinfo_proc *info = (struct kinfo_proc *)malloc(actualSize);
			if (info) {
				// This comes from looking at the Darwin xnu Kernel
				if (sysctl(mib, mibSize, info, &actualSize, NULL, 0) == 0)
					result = (info->kp_proc.p_flag & P_TRACED) ? true : false;
				free(info);
			}
		}
		return result;
	}

	static bool DumpStatsCallback(const char* dump_dir, const char* minidump_id,
																void *context, bool success)
	{
		fprintf(stderr, "Git crashed; dumped state to %s/%s.dmp\n", dump_dir, minidump_id);
		finish_stats_report(-1);
		return true;
	}

	void init_minidump(const char* path)
	{
		char *env = getenv("GIT_DISABLE_BREAKPAD");
		bool inDebugger = isInDebugger();
		bool disable = inDebugger; // disable if debugging

		mkdir(path, 02775);
		chmod(path, 02775);
		if (env) {
			if (*env == '1' || *env == 't') { // force disable
				disable = true;
			}
			else if (*env == '0' || *env == 'f') { // force enable
				disable = false;
			}
		}

		if (disable) {
			fprintf(stderr, "Breakpad disabled because inDebugger=%d or GIT_DISABLE_BREAKPAD=%s\n",
									 inDebugger, env);
			return;
		}

		std::string path_str = path;
		if (exception_handler) {
			delete exception_handler;
		}
		exception_handler = new google_breakpad::ExceptionHandler(path_str,
																															0,
																															DumpStatsCallback,
																															0,
																															true,
																															0
																															);
	}

}
