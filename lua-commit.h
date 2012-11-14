#ifndef LUA_COMMIT_H
#define LUA_COMMIT_H

struct format_commit_context;

void lua_commit_init(const char *snippet);
void lua_commit_format(struct strbuf *out, struct format_commit_context *context);

#endif
