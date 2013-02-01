#include "cache.h"
#include "lua-commit.h"
#include "commit.h"

#ifndef USE_LUA

static const char msg[] = "git was built without lua support";

void lua_commit_init(const char *unused_1)
{
	die(msg);
}

void lua_commit_format(struct strbuf *unused_1,
		       struct format_commit_context *unused_2)
{
	die(msg);
}

#else

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static lua_State *lua;

/* XXX
 * We need to access this from functions called from inside lua. Probably it
 * would be cleaner use a lua "register" to let each function access it, but I
 * haven't looked into it.
 */
static struct format_commit_context *c;

static int lua_fun_hash(lua_State *lua)
{
	lua_pushstring(lua, sha1_to_hex(c->commit->object.sha1));
	return 1;
}

static int lua_fun_abbrev(lua_State *lua)
{
	const char *hex;
	unsigned char sha1[20];

	hex = lua_tostring(lua, -1);
	if (!hex || get_sha1_hex(hex, sha1)) {
		lua_pushstring(lua, "abbrev requires a sha1");
		lua_error(lua);
	}

	lua_pushstring(lua, find_unique_abbrev(sha1, c->pretty_ctx->abbrev));
	return 1;
}

static int get_ident(lua_State *lua, const char *line, int len)
{
	struct ident_split s;

	if (split_ident_line(&s, line, len) < 0) {
		lua_pushstring(lua, "unable to parse ident line");
		lua_error(lua);
	}

	lua_createtable(lua, 0, 2);
	lua_pushstring(lua, "name");
	lua_pushlstring(lua, s.name_begin, s.name_end - s.name_begin);
	lua_settable(lua, -3);
	lua_pushstring(lua, "email");
	lua_pushlstring(lua, s.mail_begin, s.mail_end - s.mail_begin);
	lua_settable(lua, -3);

	/* XXX should also put date in the table */

	return 1;
}

static int lua_fun_author(lua_State *lua)
{
	if (!c->commit_header_parsed)
		parse_commit_header(c);
	return get_ident(lua, c->message + c->author.off, c->author.len);
}

static int lua_fun_committer(lua_State *lua)
{
	if (!c->commit_header_parsed)
		parse_commit_header(c);
	return get_ident(lua, c->message + c->committer.off, c->committer.len);
}

static int lua_fun_message(lua_State *lua)
{
	lua_pushstring(lua, c->message + c->message_off + 1);
	return 1;
}

static int lua_fun_subject(lua_State *lua)
{
	struct strbuf tmp = STRBUF_INIT;

	if (!c->commit_header_parsed)
		parse_commit_header(c);
	if (!c->commit_message_parsed)
		parse_commit_message(c);

	format_subject(&tmp, c->message + c->subject_off, " ");
	lua_pushlstring(lua, tmp.buf, tmp.len);
	return 1;
}

static int lua_fun_body(lua_State *lua)
{
	if (!c->commit_header_parsed)
		parse_commit_header(c);
	if (!c->commit_message_parsed)
		parse_commit_message(c);

	lua_pushstring(lua, c->message + c->body_off);
	return 1;
}

void lua_commit_init(const char *snippet)
{
	if (!lua) {
		lua = luaL_newstate();
		if (!lua)
			die("unable to open lua interpreter");
		luaL_openlibs(lua);

#define REG(name) do { \
	lua_pushcfunction(lua, lua_fun_##name); \
	lua_setglobal(lua, #name); \
} while(0)

		REG(hash);
		REG(abbrev);
		REG(author);
		REG(committer);
		REG(message);
		REG(subject);
		REG(body);
	}

	if (luaL_loadstring(lua, snippet))
		die("unable to load lua snippet: %s", snippet);
}

void lua_commit_format(struct strbuf *out,
		       struct format_commit_context *context)
{
	const char *ret;
	size_t len;

	c = context;

	lua_pushvalue(lua, -1);
	if (lua_pcall(lua, 0, 1, 0))
		die("lua failed: %s", lua_tostring(lua, -1));

	ret = lua_tolstring(lua, -1, &len);
	strbuf_add(out, ret, len);
	lua_pop(lua, 1);
}

#endif /* USE_LUA */
