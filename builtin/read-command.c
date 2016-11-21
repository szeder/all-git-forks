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

int cmd_read_command(int argc, const char **argv, const char *prefix)
{
  struct argv_array new_argv = ARGV_ARRAY_INIT;
  struct strbuf buf = STRBUF_INIT;

  argv_array_push(&new_argv, "git");

  while (strbuf_getline_lf(&buf, stdin) != EOF) {
    argv_array_push(&new_argv, buf.buf);
    strbuf_reset(&buf);
  }

  strbuf_release(&buf);

  return cmd_main(new_argv.argc, new_argv.argv);
}
