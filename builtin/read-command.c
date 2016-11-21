/*
 * GIT - The information manager from hell
 *
 * Copyright (C) Linus Torvalds, 2005
 */

#include "builtin.h"
#include "exec_cmd.h"
#include "string-list.h"
#include "strbuf.h"
#include "argv-array.h"
#include "parse-options.h"

static const char * const read_command_usage[] = {
	N_("git read-command [<options>]"),
	NULL
};

int cmd_read_command(int argc, const char **argv, const char *prefix)
{
  int line_terminator = '\n';
  strbuf_getline_fn getline_fn;

  struct option builtin_read_command_options[] = {
		/* Think twice before adding "--nul" synonym to this */
		OPT_SET_INT('z', NULL, &line_terminator,
			N_("commands are separated with NUL character"), '\0'),
    OPT_END()
  };

  argc = parse_options(argc, argv, prefix, builtin_read_command_options,
      read_command_usage, 0);

  struct argv_array new_argv = ARGV_ARRAY_INIT;
  struct strbuf buf = STRBUF_INIT;

  argv_array_push(&new_argv, "git");

  getline_fn = line_terminator == '\0' ? strbuf_getline_nul : strbuf_getline;

  while (getline_fn(&buf, stdin) != EOF) {
    argv_array_push(&new_argv, buf.buf);
    strbuf_reset(&buf);
  }

  strbuf_release(&buf);

  return cmd_main(new_argv.argc, new_argv.argv);
}
