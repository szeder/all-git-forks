#include "git-compat-util.h"
#include "cache.h"
#include "pkt-line.h"
#include "quote.h"
#include "refs.h"
#include "run-command.h"
#include "remote.h"
#include "connect.h"
#include "tcp.h"
#include "url.h"
#include "string-list.h"
#include "sha1-array.h"

static char *server_capabilities;
static const char *parse_feature_value(const char *, const char *, int *);

static int check_ref(const char *name, int len, unsigned int flags)
{
	if (!flags)
		return 1;

	if (len < 5 || memcmp(name, "refs/", 5))
		return 0;

	/* Skip the "refs/" part */
	name += 5;
	len -= 5;

	/* REF_NORMAL means that we don't want the magic fake tag refs */
	if ((flags & REF_NORMAL) && check_refname_format(name, 0))
		return 0;

	/* REF_HEADS means that we want regular branch heads */
	if ((flags & REF_HEADS) && !memcmp(name, "heads/", 6))
		return 1;

	/* REF_TAGS means that we want tags */
	if ((flags & REF_TAGS) && !memcmp(name, "tags/", 5))
		return 1;

	/* All type bits clear means that we are ok with anything */
	return !(flags & ~REF_NORMAL);
}

int check_ref_type(const struct ref *ref, int flags)
{
	return check_ref(ref->name, strlen(ref->name), flags);
}

static void die_initial_contact(int got_at_least_one_head)
{
	if (got_at_least_one_head)
		die("The remote end hung up upon initial contact");
	else
		die("Could not read from remote repository.\n\n"
		    "Please make sure you have the correct access rights\n"
		    "and the repository exists.");
}

static void parse_one_symref_info(struct string_list *symref, const char *val, int len)
{
	char *sym, *target;
	struct string_list_item *item;

	if (!len)
		return; /* just "symref" */
	/* e.g. "symref=HEAD:refs/heads/master" */
	sym = xmalloc(len + 1);
	memcpy(sym, val, len);
	sym[len] = '\0';
	target = strchr(sym, ':');
	if (!target)
		/* just "symref=something" */
		goto reject;
	*(target++) = '\0';
	if (check_refname_format(sym, REFNAME_ALLOW_ONELEVEL) ||
	    check_refname_format(target, REFNAME_ALLOW_ONELEVEL))
		/* "symref=bogus:pair */
		goto reject;
	item = string_list_append(symref, sym);
	item->util = target;
	return;
reject:
	free(sym);
	return;
}

static void annotate_refs_with_symref_info(struct ref *ref)
{
	struct string_list symref = STRING_LIST_INIT_DUP;
	const char *feature_list = server_capabilities;

	while (feature_list) {
		int len;
		const char *val;

		val = parse_feature_value(feature_list, "symref", &len);
		if (!val)
			break;
		parse_one_symref_info(&symref, val, len);
		feature_list = val + 1;
	}
	sort_string_list(&symref);

	for (; ref; ref = ref->next) {
		struct string_list_item *item;
		item = string_list_lookup(&symref, ref->name);
		if (!item)
			continue;
		ref->symref = xstrdup((char *)item->util);
	}
	string_list_clear(&symref, 0);
}

/*
 * Read all the refs from the other end
 */
struct ref **get_remote_heads(int in, char *src_buf, size_t src_len,
			      struct ref **list, unsigned int flags,
			      struct sha1_array *extra_have,
			      struct sha1_array *shallow_points)
{
	struct ref **orig_list = list;
	int got_at_least_one_head = 0;

	*list = NULL;
	for (;;) {
		struct ref *ref;
		unsigned char old_sha1[20];
		char *name;
		int len, name_len;
		char *buffer = packet_buffer;

		len = packet_read(in, &src_buf, &src_len,
				  packet_buffer, sizeof(packet_buffer),
				  PACKET_READ_GENTLE_ON_EOF |
				  PACKET_READ_CHOMP_NEWLINE);
		if (len < 0)
			die_initial_contact(got_at_least_one_head);

		if (!len)
			break;

		if (len > 4 && starts_with(buffer, "ERR "))
			die("remote error: %s", buffer + 4);

		if (len == 48 && starts_with(buffer, "shallow ")) {
			if (get_sha1_hex(buffer + 8, old_sha1))
				die("protocol error: expected shallow sha-1, got '%s'", buffer + 8);
			if (!shallow_points)
				die("repository on the other end cannot be shallow");
			sha1_array_append(shallow_points, old_sha1);
			continue;
		}

		if (len < 42 || get_sha1_hex(buffer, old_sha1) || buffer[40] != ' ')
			die("protocol error: expected sha/ref, got '%s'", buffer);
		name = buffer + 41;

		name_len = strlen(name);
		if (len != name_len + 41) {
			free(server_capabilities);
			server_capabilities = xstrdup(name + name_len + 1);
		}

		if (extra_have &&
		    name_len == 5 && !memcmp(".have", name, 5)) {
			sha1_array_append(extra_have, old_sha1);
			continue;
		}

		if (!check_ref(name, name_len, flags))
			continue;
		ref = alloc_ref(buffer + 41);
		hashcpy(ref->old_sha1, old_sha1);
		*list = ref;
		list = &ref->next;
		got_at_least_one_head = 1;
	}

	annotate_refs_with_symref_info(*orig_list);

	return list;
}

static const char *parse_feature_value(const char *feature_list, const char *feature, int *lenp)
{
	int len;

	if (!feature_list)
		return NULL;

	len = strlen(feature);
	while (*feature_list) {
		const char *found = strstr(feature_list, feature);
		if (!found)
			return NULL;
		if (feature_list == found || isspace(found[-1])) {
			const char *value = found + len;
			/* feature with no value (e.g., "thin-pack") */
			if (!*value || isspace(*value)) {
				if (lenp)
					*lenp = 0;
				return value;
			}
			/* feature with a value (e.g., "agent=git/1.2.3") */
			else if (*value == '=') {
				value++;
				if (lenp)
					*lenp = strcspn(value, " \t\n");
				return value;
			}
			/*
			 * otherwise we matched a substring of another feature;
			 * keep looking
			 */
		}
		feature_list = found + 1;
	}
	return NULL;
}

int parse_feature_request(const char *feature_list, const char *feature)
{
	return !!parse_feature_value(feature_list, feature, NULL);
}

const char *server_feature_value(const char *feature, int *len)
{
	return parse_feature_value(server_capabilities, feature, len);
}

int server_supports(const char *feature)
{
	return !!server_feature_value(feature, NULL);
}

enum protocol {
	PROTO_LOCAL = 1,
	PROTO_FILE,
	PROTO_SSH,
	PROTO_GIT
};

int url_is_local_not_ssh(const char *url)
{
	const char *colon = strchr(url, ':');
	const char *slash = strchr(url, '/');
	return !colon || (slash && slash < colon) ||
		has_dos_drive_prefix(url);
}

static const char *prot_name(enum protocol protocol)
{
	switch (protocol) {
		case PROTO_LOCAL:
		case PROTO_FILE:
			return "file";
		case PROTO_SSH:
			return "ssh";
		case PROTO_GIT:
			return "git";
		default:
			return "unkown protocol";
	}
}

static enum protocol get_protocol(const char *name)
{
	if (!strcmp(name, "ssh"))
		return PROTO_SSH;
	if (!strcmp(name, "git"))
		return PROTO_GIT;
	if (!strcmp(name, "git+ssh"))
		return PROTO_SSH;
	if (!strcmp(name, "ssh+git"))
		return PROTO_SSH;
	if (!strcmp(name, "file"))
		return PROTO_FILE;
	die("I don't handle protocol '%s'", name);
}

static const char *get_port_numeric(const char *p)
{
	char *end;
	if (p) {
		long port = strtol(p + 1, &end, 10);
		if (end != p + 1 && *end == '\0' && 0 <= port && port < 65536) {
			return p;
		}
	}

	return NULL;
}

/*
 * Extract protocol and relevant parts from the specified connection URL.
 * The caller must free() the returned strings.
 */
static enum protocol parse_connect_url(const char *url_orig, char **ret_host,
				       char **ret_path)
{
	char *url;
	char *host, *path;
	char *end;
	int separator = '/';
	enum protocol protocol = PROTO_LOCAL;

	if (is_url(url_orig))
		url = url_decode(url_orig);
	else
		url = xstrdup(url_orig);

	host = strstr(url, "://");
	if (host) {
		*host = '\0';
		protocol = get_protocol(url);
		host += 3;
	} else {
		host = url;
		if (!url_is_local_not_ssh(url)) {
			protocol = PROTO_SSH;
			separator = ':';
		}
	}

	/*
	 * Don't do destructive transforms as protocol code does
	 * '[]' unwrapping in get_host_and_port()
	 */
	if (host[0] == '[') {
		end = strchr(host + 1, ']');
		if (end) {
			end++;
		} else
			end = host;
	} else
		end = host;

	if (protocol == PROTO_LOCAL)
		path = end;
	else if (protocol == PROTO_FILE && has_dos_drive_prefix(end))
		path = end; /* "file://$(pwd)" may be "file://C:/projects/repo" */
	else
		path = strchr(end, separator);

	if (!path || !*path)
		die("No path specified. See 'man git-pull' for valid url syntax");

	/*
	 * null-terminate hostname and point path to ~ for URL's like this:
	 *    ssh://host.xz/~user/repo
	 */

	end = path; /* Need to \0 terminate host here */
	if (separator == ':')
		path++; /* path starts after ':' */
	if (protocol == PROTO_GIT || protocol == PROTO_SSH) {
		if (path[1] == '~')
			path++;
	}

	path = xstrdup(path);
	*end = '\0';

	*ret_host = xstrdup(host);
	*ret_path = path;
	free(url);
	return protocol;
}

static struct child_process no_fork;

/*
 * This returns a dummy child_process if the transport protocol does not
 * need fork(2), or a struct child_process object if it does.  Once done,
 * finish the connection with finish_connect() with the value returned from
 * this function (it is safe to call finish_connect() with NULL to support
 * the former case).
 *
 * If it returns, the connect is successful; it just dies on errors (this
 * will hopefully be changed in a libification effort, to return NULL when
 * the connection failed).
 */
struct child_process *git_connect(int fd[2], const char *url,
				  const char *prog, int flags)
{
	char *hostandport, *path;
	struct child_process *conn = &no_fork;
	enum protocol protocol;
	const char **arg;
	struct strbuf cmd = STRBUF_INIT;

	/* Without this we cannot rely on waitpid() to tell
	 * what happened to our children.
	 */
	signal(SIGCHLD, SIG_DFL);

	protocol = parse_connect_url(url, &hostandport, &path);
	if (flags & CONNECT_DIAG_URL) {
		printf("Diag: url=%s\n", url ? url : "NULL");
		printf("Diag: protocol=%s\n", prot_name(protocol));
		printf("Diag: hostandport=%s\n", hostandport ? hostandport : "NULL");
		printf("Diag: path=%s\n", path ? path : "NULL");
		conn = NULL;
	} else if (protocol == PROTO_GIT) {
		/* These underlying connection commands die() if they
		 * cannot connect.
		 */
		char *target_host = xstrdup(hostandport);
		if (git_use_proxy(hostandport))
			conn = git_proxy_connect(fd, hostandport);
		else
			git_tcp_connect(fd, hostandport, flags);
		/*
		 * Separate original protocol components prog and path
		 * from extended host header with a NUL byte.
		 *
		 * Note: Do not add any other headers here!  Doing so
		 * will cause older git-daemon servers to crash.
		 */
		packet_write(fd[1],
			     "%s %s%chost=%s%c",
			     prog, path, 0,
			     target_host, 0);
		free(target_host);
	} else {
		conn = xcalloc(1, sizeof(*conn));

		strbuf_addstr(&cmd, prog);
		strbuf_addch(&cmd, ' ');
		sq_quote_buf(&cmd, path);

		conn->in = conn->out = -1;
		conn->argv = arg = xcalloc(7, sizeof(*arg));
		if (protocol == PROTO_SSH) {
			const char *ssh = getenv("GIT_SSH");
			int putty = ssh && strcasestr(ssh, "plink");
			char *ssh_host = hostandport;
			const char *port = NULL;
			get_host_and_port(&ssh_host, &port);
			port = get_port_numeric(port);

			if (!ssh) ssh = "ssh";

			*arg++ = ssh;
			if (putty && !strcasestr(ssh, "tortoiseplink"))
				*arg++ = "-batch";
			if (port) {
				/* P is for PuTTY, p is for OpenSSH */
				*arg++ = putty ? "-P" : "-p";
				*arg++ = port;
			}
			*arg++ = ssh_host;
		} else {
			/* remove repo-local variables from the environment */
			conn->env = local_repo_env;
			conn->use_shell = 1;
		}
		*arg++ = cmd.buf;
		*arg = NULL;

		if (start_command(conn))
			die("unable to fork");

		fd[0] = conn->out; /* read from child's stdout */
		fd[1] = conn->in;  /* write to child's stdin */
		strbuf_release(&cmd);
	}
	free(hostandport);
	free(path);
	return conn;
}

int git_connection_is_socket(struct child_process *conn)
{
	return conn == &no_fork;
}

int finish_connect(struct child_process *conn)
{
	int code;
	if (!conn || git_connection_is_socket(conn))
		return 0;

	code = finish_command(conn);
	free(conn->argv);
	free(conn);
	return code;
}
