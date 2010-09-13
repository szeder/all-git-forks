/*
 * A completely rubbish MUA, only capable of clogging up Linus' inbox
 */

#include "cache.h"
#include "object.h"
#include "commit.h"
#include "diff.h"
#include "revision.h"
#include "builtin.h"
#include "parse-options.h"
#include "exec_cmd.h"
#include "string-list.h"
#include "run-command.h"

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

static const char *const send_email_usage[] = {
	"git send-email [options] [--] "
	"<file | directory | rev-list options>...",
	NULL
};

static int verbose = 0, smtp_debug = 0, format_patch = -1;
static struct string_list to_rcpts, cc_rcpts, bcc_rcpts;

struct smtp_socket {
	int fd;
#ifndef NO_OPENSSL
	SSL *ssl;
#endif
};

static void die_sockerr(struct smtp_socket *sock, int ret, const char *fmt, ...)
{
	char msg[4096];
	va_list params;

	va_start(params, fmt);
	vsnprintf(msg, sizeof(msg), fmt, params);
	va_end(params);

#ifndef NO_OPENSSL
	if (sock->ssl) {
		if (SSL_get_error(sock->ssl, ret) == SSL_ERROR_SYSCALL)
			die_errno("%s", msg);
		die("%s: %s\n", msg, ERR_error_string(ERR_get_error(), NULL));
	}
#else
	die_errno("%s", msg);
#endif
}

static int socket_write(struct smtp_socket *sock, const void *data, int len)
{
	int n;
#ifndef NO_OPENSSL
	if (sock->ssl)
		n = SSL_write(sock->ssl, data, len);
	else
#endif
		n = write_in_full(sock->fd, data, len);

	if (smtp_debug) {
		int i;
		fputs("C: ", stdout);
		for (i = 0; i < len - 2; ++i) {
			int c = ((char*)data)[i];
			if ((c > 31 && c < 127) || c == '\t')
				putchar(c);
			else if (c == '\n')
				fputs("\nC: ", stdout);
			else if (c != '\r')
				printf("<%02X>", c);
		}
		fputs("\n", stdout);
	}

	if (n <= 0)
		die_sockerr(sock, n, "write() failed");

	return n;
}

static int socket_read(struct smtp_socket *sock, void *data, int len)
{
	int n;
#ifndef NO_OPENSSL
	if (sock->ssl)
		n = SSL_read(sock->ssl, data, len);
	else
#endif
		n = xread(sock->fd, data, len);

	if (n <= 0)
		die_sockerr(sock, n, "read() failed");

	return n;
}

#ifndef NO_IPV6

/* use IPv6 API */
static int connect_socket(const char *host, int nport)
{
	int gai, sockfd = -1;
	struct addrinfo hints, *ai0, *ai;
	char portstr[6];

	snprintf(portstr, sizeof(portstr), "%d", nport);

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	if (verbose)
		fprintf(stderr, "Looking up %s ... ", host);

	gai = getaddrinfo(host, portstr, &hints, &ai);
	if (gai)
		die("Unable to look up %s (port %d) (%s)", host, nport, gai_strerror(gai));

	if (verbose)
		fprintf(stderr, "done.\nConnecting to %s (port %d) ... ", host, nport);

	for (ai0 = ai; ai; ai = ai->ai_next) {
		char addr[NI_MAXHOST];

		sockfd = socket(ai->ai_family, ai->ai_socktype,
			   ai->ai_protocol);
		if (sockfd < 0)
			continue;

		getnameinfo(ai->ai_addr, ai->ai_addrlen, addr,
			    sizeof(addr), NULL, 0, NI_NUMERICHOST);

		if (verbose)
			fprintf(stderr, "Connecting to [%s]:%s... ", addr, portstr);

		if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) >= 0)
			break;

		/* try next address in list, if any */
		perror(host);
		close(sockfd);
		sockfd = -1;
	}

	freeaddrinfo(ai0);

	if (sockfd < 0)
		die_errno("unable to connect a socket (%s)", strerror(errno));

	if (verbose)
		fprintf(stderr, "done.\n");

	return sockfd;
}

#else /* NO_IPV6 */

/* use IPv4 API */
static int connect_socket(const char *host, int nport)
{
	int sockfd = -1;
	struct sockaddr_in sa;
	struct hostent *he;
	char **ap;

	if (verbose)
		fprintf(stderr, "Looking up %s ... ", host);

	he = gethostbyname(host);
	if (!he)
		die("Unable to look up %s (%s)", host, hstrerror(h_errno));

	if (verbose)
		fprintf(stderr, "got host: %s\n", he->h_name);

	ap = he->h_addr_list;
	while (*ap) {
		memset(&sa, 0, sizeof sa);
		sa.sin_family = he->h_addrtype;
		sa.sin_port = htons(nport);
		memcpy(&sa.sin_addr, *ap, he->h_length);

		if (verbose) {
			fprintf(stderr, "Connecting to %d.%d.%d.%d:%d... ",
			    (unsigned char)(*ap)[0],
			    (unsigned char)(*ap)[1],
			    (unsigned char)(*ap)[2],
			    (unsigned char)(*ap)[3],
			    nport);
		}

		sockfd = socket(he->h_addrtype, SOCK_STREAM, 0);
		if (sockfd < 0) {
			perror(host);
			continue;
		}

		if (connect(sockfd, (struct sockaddr *)&sa, sizeof sa) >= 0)
			break;

		/* try next address in list, if any */
		perror(host);
		close(sockfd);
		sockfd = -1;
		++ap;
	}
	if (sockfd < 0)
		die("Could not connect to '%s:%d'", host, nport);

	return sockfd;
}
#endif

#ifndef NO_OPENSSL
static void connect_ssl(struct smtp_socket *sock)
{
	int ret;
	SSL_CTX *ctx;
	SSL_library_init();
	SSL_load_error_strings();
	ctx = SSL_CTX_new(SSLv3_client_method());
	sock->ssl = SSL_new(ctx);
	if (!sock->ssl)
		die("SSL_new: %s", ERR_error_string(ERR_get_error(), 0));

	if (!SSL_set_fd(sock->ssl, sock->fd))
		die("SSL_set_fd: %s", ERR_error_string(ERR_get_error(), 0));

	ret = SSL_connect(sock->ssl);
	if (ret != 1)
		die_sockerr(sock, ret, "SSL_connect() failed");
}
#endif

static void write_command(struct smtp_socket *sock, char *fmt, ...)
{
	int n;
	char buffer[512 + 1]; /* max command line length + termination */
	va_list ap;

	va_start(ap, fmt);
	n = vsnprintf(buffer, 512 - 2, fmt, ap);
	va_end(ap);

	buffer[n] = '\r';
	buffer[n+1] = '\n';

	socket_write(sock, buffer, n + 2);
}

static int get_reply(struct smtp_socket *sock, struct string_list *reply)
{
	char buf[512 + 1]; /* max reply-line length is 512 bytes */
	int n;

	n = socket_read(sock, buf, 512);
	do {
		char *line = buf, *next_line;
		while (NULL != (next_line = memchr(line, '\n', n))) {
			size_t line_len = (size_t)next_line - (size_t)line;

			/*
			 * the minimum valid reply line length for SMTP is 5
			 * bytes in the form "XYZ<CRLF>", where XYZ is the
			 * return-code
			 */
			if (line_len < 5 || line[line_len - 1] != '\r') {
				line[line_len] = '\0';
				warning("Invalid reply-line: \"%s\"", line);
			}

			line[line_len - 1] = '\0';
			string_list_append(reply, line);

			if (smtp_debug)
				printf("S: %s\n", line);

			if (line[3] != '-') /* last line in reply */
				return line[0] - '0';

			/* move on to the next line */
			line = next_line + 1;
			n -= line_len + 1;
		}

		/* move the rest of the buffer to the start */
		if (n > 0)
			memmove(buf, line, n);

		n += socket_read(sock, buf + n, 512 - n);
	} while (1);
}

static void die_reply(const char *prefix, struct string_list *reply)
{
	int i;
	fprintf(stderr, "fatal: %s", prefix);
	for (i = 0; i < reply->nr; ++i)
		fprintf(stderr, "%s\n", reply->items[i].string);
	exit(1);
}

/* get the reply, and die if the first character of the reply code is not
   the expected value */
static void demand_reply_code(struct smtp_socket *sock, int expected)
{
	struct string_list reply;
	int ret;

	memset(&reply, 0, sizeof(reply));
	reply.strdup_strings = 1;

	ret = get_reply(sock, &reply);
	if (ret == expected) {
		string_list_clear(&reply, 0);
		return;
	}

	die_reply("Unexpected reply:\n", &reply);
}

static int send_helo(struct smtp_socket *sock, const char *hostname, struct string_list *reply)
{
	int reply_code, use_esmtp = 1;
	write_command(sock, "EHLO %s", hostname);

	reply_code = get_reply(sock, reply);
	if (reply_code == 5) {
		use_esmtp = 0;
		string_list_clear(reply, 0);
		write_command(sock, "HELO %s", hostname);
		reply_code = get_reply(sock, reply);
	}

	if (reply_code != 2)
		die_reply("HELO Unexpected reply:\n", reply);

	return use_esmtp;
}

static int add_rcpt(const char *var, const char *value)
{
	if (strchr(value, ','))
		die("Comma in --%s entry: %s", var, value);

	if (!strcmp(var, "to"))
		string_list_append(&to_rcpts, value);
	else if (!strcmp(var, "cc"))
		string_list_append(&cc_rcpts, value);
	else if (!strcmp(var, "bcc"))
		string_list_append(&bcc_rcpts, value);
	else
		die("unexpected option %s", var);

	return 0;
}

static int xadd_rcpt(const struct option *opt, const char *arg, int unset)
{
	return add_rcpt(opt->long_name, arg);
}

static const char *smtp_server = NULL, *port = NULL,
    *smtp_domain = NULL, *smtp_user = NULL, *smtp_pass = NULL;

static int git_send_email_config(const char *var, const char *value, void *cb)
{
	if (prefixcmp(var, "sendemail."))
		return git_color_default_config(var, value, cb);

	var += 10;
	if (!strcmp(var, "smtpserver"))
		return git_config_string(&smtp_server, var, value);
	if (!strcmp(var, "smtpserverport"))
		return git_config_string(&port, var, value);
	if (!strcmp(var, "smtpuser"))
		return git_config_string(&smtp_user, var, value);
	if (!strcmp(var, "smtppass"))
		return git_config_string(&smtp_pass, var, value);
	if (!strcmp(var, "to") || !strcmp(var, "cc") || !strcmp(var, "bcc"))
		return add_rcpt(var, value);

	return 0;
}

static char *extract_mailbox(const char *str)
{
	char *qb = strchr(str, '<'), *qe, *ret;
	if (!qb)
		return xstrdup(str);

	qe = strchr(qb, '>');
	if (!qe)
		die("malformed address '%s'", str);

	ret = (char *)xmalloc(qe - qb);
	strncpy(ret, qb + 1, qe - qb);
	ret[qe - qb - 1] = '\0';
	return ret;
}

static void auth_plain(struct smtp_socket *sock, const char *user, const char *pass)
{
	struct strbuf auth = STRBUF_INIT, auth64 = STRBUF_INIT;

	strbuf_addch(&auth, '\0');
	strbuf_addstr(&auth, user);
	strbuf_addch(&auth, '\0');
	strbuf_addstr(&auth, pass);
	strbuf_addch(&auth, '\0');

	encode_64(&auth64, auth.buf, auth.len);
	write_command(sock, "AUTH PLAIN %s", auth64.buf);
	demand_reply_code(sock, 2);
}

static void auth_login(struct smtp_socket *sock, const char *user, const char *pass)
{
	struct strbuf sb = STRBUF_INIT;

	write_command(sock, "AUTH LOGIN");
	demand_reply_code(sock, 3);

	encode_64(&sb, user, strlen(user));
	write_command(sock, "%s", sb.buf);
	strbuf_release(&sb);
	demand_reply_code(sock, 3);

	encode_64(&sb, pass, strlen(pass));
	write_command(sock, "%s", sb.buf);
	strbuf_release(&sb);
	demand_reply_code(sock, 2);
}

static char *ask(const char *prompt, const char *def, const char *valid_re)
{
	int i = 0;
	char *ret;
	regex_t re;

	if (!isatty(0) && !isatty(1))
		return def ? xstrdup(def) : NULL;

	if (valid_re && regcomp(&re, valid_re, REG_NOSUB))
		valid_re = NULL;

	while (i++ < 10) {
		char *resp;
		if (want_color(GIT_COLOR_AUTO)) {
			struct strbuf sb = STRBUF_INIT;
			strbuf_addf(&sb, GIT_COLOR_CYAN "%s" GIT_COLOR_RESET,
			    prompt);
			resp = readline(sb.buf);
			strbuf_release(&sb);
		} else
			resp = readline(prompt);

		if (!resp) {
			fputc('\n', stdout);
			ret = def ? xstrdup(def) : NULL;
			goto done;
		}
		if (!strlen(resp)) {
			free(resp);
			if (def)
				ret = xstrdup(def);
			else
				ret = NULL;
			goto done;
		}
		if (!valid_re || regexec(&re, resp, 0, NULL, 0)) {
			ret = resp;
			goto done;
		}
		free(resp);
	}

	ret = NULL;
done:
	if (valid_re)
		regfree(&re);
	return ret;
}

static int check_file_rev_conflict(const char *arg)
{
	struct rev_info revs;

	/* needs to clone the string, since
	   handle_revision_arg modifies the input */
	char *temp_arg = xstrdup(arg);
	init_revisions(&revs, NULL);
	if (!handle_revision_arg(temp_arg, &revs, 0, 1)) {
		if (format_patch >= 0) {
			free(temp_arg);
			return !format_patch;
		}

	die(
"File '%s' exists but it could also be the range of commits\n"
"to produce patches for.  Please disambiguate by...\n"
"\n"
"    * Saying './%s' if you mean a file; or\n"
"    * Giving --format-patch option if you mean a range.\n", arg, arg);
	}
	return 0;
}

int valid_fqdn(const char *domain)
{
#ifdef __APPLE__
	/*
	 * Mac OS X (Bonjour) use domain names ending with .local, which will
	 * fail for DNS lookups. So filter out these.
	 *
	 */
	size_t len = strlen(domain);
	if (len >= 6 && strcmp(domain + len - 5, ".local"))
		return 0;
#endif
	return !!strchr(domain, '.');
}

const char *maildomain()
{
	static char name[256];
	struct addrinfo *ai;

#ifdef HAVE_UNAME
	struct utsname un;

	if (uname(&un))
		die_errno("uname");
	if (valid_fqdn(un.nodename))
		return un.nodename;
#endif

	gethostname(name, sizeof(name));
	if (valid_fqdn(name))
		return name;

	if (!getaddrinfo(name, NULL, NULL, &ai))
		while (ai) {
			if (!getnameinfo(ai->ai_addr, ai->ai_addrlen,
			    name, sizeof(name), NULL, 0, 0) &&
			    valid_fqdn(name))
				return name;
			ai = ai->ai_next;
		}

	return "localhost.localdomain";
}

int main(int argc, const char **argv)
{
	char hostname[256];
	struct smtp_socket sock = { 0 };
	struct string_list helo_reply = { 0 };
	int i, use_esmtp, nongit_ok, prompting;
	struct string_list files = { 0 };

	int quiet = 0, dry_run = 0, nport = -1, thread = 1, force = 0;
	const char *sender = NULL, *subject = NULL, *reply_to = NULL;
	const char *smtp_encryption = "";

	struct option options[] = {
		OPT_GROUP("Composing:"),
		OPT_STRING(0, "from", &sender, "str", "Email From:"),
		OPT_CALLBACK(0, "to", NULL, "str", "Email To:", xadd_rcpt),
		OPT_CALLBACK(0, "cc", NULL, "str", "Email Cc:", xadd_rcpt),
		OPT_CALLBACK(0, "bcc", NULL, "str", "Email Bcc:", xadd_rcpt),
		OPT_STRING(0, "subject", &subject, "str", "Email \"Subject:\""),
		OPT_STRING(0, "in-reply-to", &reply_to, "str", "Email \"In-Reply-To:\""),
		/* TODO: OPT_BOOLEAN(0, "annotate", &annotate, "Review each patch that will be sent in an editor."), */
		/* TODO: OPT_BOOLEAN(0, "compose", &compose, "Open an editor for introduction."), */
		/* TODO: OPT_STRING(0, "8bit-encoding", &encoding, "str", "Encoding to assume 8bit mails if undeclared"), */

		OPT_GROUP("Sending:"),
		/* TODO: OPT_STRING(0, "envelope-sender", &envelope_sender, "str", "Email envelope sender."), */
		OPT_STRING(0, "smtp-server", &smtp_server, "str", "SMTP server to send through"),
		/* TODO: OPT_STRING(0, "smtp-server-option", &smtp_server_opt, "str", "Outgoing SMTP server option to use."), */
		OPT_STRING(0, "smtp-server-port", &port, "port", "Outgoing SMTP server port"),
		OPT_STRING(0, "smtp-user", &smtp_user, "user", "SMTP-AUTH username"),
		OPT_STRING(0, "smtp-pass", &smtp_pass, "password", "SMTP-AUTH password"),
		OPT_STRING(0, "smtp-encryption", &smtp_encryption, "str", "SMTP encrption"),
		/* TODO: smtp-ssl (alias for --smtp-encryption ssl) */
		OPT_STRING(0, "smtp-domain", &smtp_domain, "str", "domain for HELO SMTP command"),
		OPT_BOOLEAN(0, "smtp-debug", &smtp_debug, "print raw protocol to stdout"),

		OPT_GROUP("Automating:"),
		/* TODO: identity */
		/* TODO: to-cmd */
		/* TODO: cc-cmd */
		/* TODO: suppress-cc */
		/* TODO: signed-off-by-cc */
		/* TODO: suppress-from */
		/* TODO: chain-reply-to */
		OPT_BOOLEAN(0, "thread", &thread, "Use In-Reply-To: field. Default on."),

		OPT_GROUP("Administering:"),
		/* TODO: confirm */
		OPT__QUIET(&quiet, "Output one line of info per email."),
		OPT__DRY_RUN(&dry_run, "Don't actually send the emails."),
		/* TODO: validate */
		OPT_BOOLEAN(0, "format-patch", &format_patch, "understand any non optional arguments as `git format-patch` ones."),
		OPT_BOOLEAN(0, "force", &force, "Send even if safety checks would prevent it."),
		OPT__VERBOSE(&verbose, "be verbose"),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);
	setup_git_directory_gently(&nongit_ok);
	git_config(git_send_email_config, NULL);
	argc = parse_options(argc, argv, NULL, options, send_email_usage, 0);

	if (reply_to) {
		char *p = strchr(reply_to, '<');
		if (p)
			reply_to = p + 1;
		p = strrchr(reply_to, '>');
		if (p)
			*p = '\0';
		printf("<%s>\n", reply_to);
	}

	for (i = 0; i < argc; ++i) {
		struct stat st;
		const char *arg = argv[i];

		if (!strcmp(arg, "--")) {
			/* fill in */
			break;
		} else if (!lstat(arg, &st) && S_ISDIR(st.st_mode)) {
			DIR *dir;
			struct dirent *de;

			dir = opendir(arg);
			if (!dir)
				die("Failed to opendir '%s': %s", arg, strerror(errno));

			while ((de = readdir(dir)) != NULL) {
				char path[PATH_MAX];
				strcpy(path, arg);
				strcat(path, "/");
				strcat(path, de->d_name);
				if (stat(path, &st))
					die_errno("lstat '%s'", path);

				if (S_ISREG(st.st_mode))
					string_list_append(&files, xstrdup(path));
			}
			closedir(dir);
		} else if (!access(arg, R_OK))
			string_list_append(&files, arg);
		else {
			struct rev_info revs;
			/* needs to clone the string, since
			   handle_revision_arg modifies the input */
			char *temp_arg = xstrdup(arg);
			init_revisions(&revs, NULL);
			if (!handle_revision_arg(temp_arg, &revs, 0, 1)) {
				char *tmp;

				if (!lstat(arg, &st))
					die(
"File '%s' exists but it could also be the range of commits\n"
"to produce patches for.  Please disambiguate by...\n"
"\n"
"    * Saying './%s' if you mean a file; or\n"
"    * Giving --format-patch option if you mean a range.\n",
					arg, arg);

				printf("'%s' is a revision-range\n", arg);

				tmp = xstrdup(getenv("TMPDIR"));
				if (!tmp)
					tmp = xstrdup("/tmp");

				printf("wat: %s\n", git_path("send-email-XXXXXX"));
				tmp = mkdtemp(xstrdup(git_path("send-email-XXXXXX")));
				if (!tmp)
					die("mkdtemp: %s", strerror(errno));
				printf("tmpdir = '%s'\n", tmp);
				rmdir(tmp);
			}
			free(temp_arg);
		}
	}

	if (files.nr) {
		if (!quiet)
			for (i = 0; i < files.nr; ++i)
				puts(files.items[i].string);
	} else {
		fprintf(stderr, "\nNo patch files specified!\n\n");
		usage_with_options(send_email_usage, options);
	}

	/* TODO: compose
	if (compose) {
	} else if (annotate) {
		do_edit(files);
	} */
	/* TODO: check encoding */

	/* TODO: write get_patch_subject
	if (!force)
		for (i = 0; i < files.nr; ++i)
			if (!strcmp(get_patch_subject(files.items[i].string), "*** SUBJECT HERE ***"))
				die(
"Refusing to send because the patch\n"
"%s\n"
"has the template subject '*** SUBJECT HERE ***.\n"
"Pass --force if you really want to send.\n",
				    files.items[i].string);
	*/

	prompting = 0;
	if (!sender) {
		char temp[1000];
		sender = git_author_info(IDENT_NO_DATE);
		if (!sender)
			sender = git_committer_info(IDENT_NO_DATE);

		snprintf(temp, sizeof(temp),
		    "Who should the emails appear to be from? [%s] ",
		    sender);
		sender = ask(temp, sender, NULL);
		printf("Emails will be sent from: '%s'\n", sender);
		prompting++;
	}

	if (!to_rcpts.nr) {
		char *to = ask("Who should the emails be sent to? ", NULL, NULL);
		if (to && strlen(to) > 0)
			string_list_append(&to_rcpts, to);
		else
			free(to);
		prompting++;
	}

	/* TODO: expand alliases */

	if (thread && !reply_to && prompting)
		reply_to = ask("Message-ID to be used as In-Reply-To for the first email? ",
		    NULL, NULL);

	if (!smtp_server) {
		if (!access("/usr/sbin/sendmail", X_OK))
			smtp_server = "/usr/sbin/sendmail";
		else if (!access("/usr/lib/sendmail", X_OK))
			smtp_server = "/usr/lib/sendmail";
		else
			smtp_server = "localhost";
	}

#if 0
	for (i = 0; i < files.nr; ++i) {
		const char *fname = files.items[i].string;
		FILE *fp;
		struct strbuf line = STRBUF_INIT;

		if (verbose)
			fprintf(stderr, "Opening \"%s\"\n", fname);

		fp = fopen(fname, "r");
		if (!fp)
			die("Failed to open file %s\n", fname);

		printf("*** PARSING HEADERS\n");
		while (strbuf_getline(&line, fp, '\n') != EOF) {
			if (!prefixcmp(line.buf, "From "))
				continue;

			printf("line: \"%s\"\n", line.buf);
			if (!line.len)
				break;
		}

		printf("*** PARSING MESSAGE\n");
		while (strbuf_getline(&line, fp, '\n') != EOF)
			printf("line: \"%s\"\n", line.buf);

		fclose(fp);
		strbuf_release(&line);
	}
#endif

	if (is_absolute_path(smtp_server)) {
		const char *data =
			"Subject: Test!\r\n"
			"Content-Type: text/plain; charset=UTF-8\r\n"
			"Content-Transfer-Encoding: quoted-printable\r\n"
			"\r\n"
			"Hello there! \"=C3=85\"\r\n";

		const char *argv[] = { smtp_server, "-i", "kusmabite@gmail.com", NULL };
		struct child_process cld;
		int status;

		memset(&cld, 0, sizeof(cld));
		cld.argv = argv;
		cld.in = -1;
		if (start_command(&cld))
			die("unable to fork '%s'", smtp_server);
		write_in_full(cld.in, data, strlen(data));
		close(cld.in);
		status = finish_command(&cld);
		if (status)
			exit(status);

		exit(0); /* hack */
	} else {
		if (NULL != port) {
			char *ep;
			nport = strtoul(port, &ep, 10);
			if (ep == port || *ep) {
				struct servent *se = getservbyname(port, "tcp");
				if (!se)
					die("Unknown port %s", port);
				nport = se->s_port;
			}
		}

		if (nport < 0) {
			if      (!strcmp(smtp_encryption, "tls")) nport = 587;
			else if (!strcmp(smtp_encryption, "ssl")) nport = 465;
			else nport = 25;
		}

		sock.fd = connect_socket(smtp_server, nport);

		if (!strcmp(smtp_encryption, "ssl"))
#ifndef NO_OPENSSL
			connect_ssl(&sock);
#else
			die("OpenSSL not available");
#endif

		demand_reply_code(&sock, 2);

		gethostname(hostname, 256);
		helo_reply.strdup_strings = 1;
		use_esmtp = send_helo(&sock, hostname, &helo_reply);

		if (!strcmp(smtp_encryption, "tls")) {
#ifndef NO_OPENSSL
			write_command(&sock, "STARTTLS");
			demand_reply_code(&sock, 2);
			connect_ssl(&sock);

			/* extensions can change after STARTTLS */
			string_list_clear(&helo_reply, 0);
			use_esmtp = send_helo(&sock, hostname, &helo_reply);
#else
			die("OpenSSL not available");
#endif
		}

		if (smtp_user) {
			const char *auth_line = NULL;
			if (!smtp_pass)
				smtp_pass = getpass("Password: ");
			if (!smtp_pass)
				die_errno("Could not read password");


			for (i = 0; i < helo_reply.nr; ++i)
				if (!prefixcmp(helo_reply.items[i].string + 4, "AUTH "))
					auth_line = helo_reply.items[i].string;

			if (auth_line) {
				if (strstr(auth_line, " PLAIN"))
					auth_plain(&sock, smtp_user, smtp_pass);
				else if (strstr(auth_line, " LOGIN"))
					auth_login(&sock, smtp_user, smtp_pass);
				else
					die("No appropriate SASL mechanism found");
			}
			else
				die("username specified, but SMTP server does not support the AUTH extension");
		}
	}

#if 1
	{
		char *mbox = extract_mailbox(sender);
		write_command(&sock, "MAIL FROM:<%s>", mbox);
		free(mbox);
		demand_reply_code(&sock, 2);
	}

	printf("to_rcpts: %d\n", to_rcpts.nr);
	for (i = 0; i < to_rcpts.nr; ++i) {
		write_command(&sock, "RCPT TO:<%s>", to_rcpts.items[i].string);
		demand_reply_code(&sock, 2);
	}

	write_command(&sock, "DATA");
	demand_reply_code(&sock, 3);

	{
		struct strbuf headers = STRBUF_INIT;

		/* build From-field */
		strbuf_addf(&headers, "From: %s\r\n", sender);

		/* build To-field*/
		strbuf_addstr(&headers, "To:");
		for (i = 0; i < to_rcpts.nr; ++i)
			strbuf_addf(&headers, "\t%s%s\r\n",
				to_rcpts.items[i].string,
				i != to_rcpts.nr - 1 ? "," : "");

		/* build Cc-field*/
		if (cc_rcpts.nr > 1) {
			strbuf_addstr(&headers, "Cc:");
			for (i = 0; i < cc_rcpts.nr; ++i)
				strbuf_addf(&headers, "\t%s%s\r\n",
					cc_rcpts.items[i].string,
					i != cc_rcpts.nr - 1 ? "," : "");
		}

		strbuf_addf(&headers, "Subject: %s\r\n", "git-send-email.c test");
		strbuf_addstr(&headers, "X-Mailer: " GIT_XMAILER "\r\n");

		socket_write(&sock, headers.buf, headers.len);
	}

	{
		const char *data =
			"Subject: Test!\r\n"
			"Content-Type: text/plain; charset=UTF-8\r\n"
			"Content-Transfer-Encoding: quoted-printable\r\n"
			"\r\n"
			"Hello there! \"=C3=85\"\r\n";

		socket_write(&sock, data, strlen(data));
		socket_write(&sock, "\r\n.\r\n", 5);
		demand_reply_code(&sock, 2);
	}
#endif

	if (verbose)
		fprintf(stderr, "Closing connection\n");
	write_command(&sock, "QUIT");
	demand_reply_code(&sock, 2);

	return 0;
}
