/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014, Ronnie Sahlberg <sahlberg@google.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <tdb.h>

#define REFS_TDB "refs.tdb"
static TDB_CONTEXT *refs_tdb;
#define LOGS_TDB "logs.tdb"
static TDB_CONTEXT *logs_tdb;

static int usage(void)
{
	fprintf(stderr, "Usage: refsd <socket> <dbdir> <logfile>\n");
	exit(1);
}

static void setup_logging(char *logfile)
{
	int fd;

	if ((fd = open(logfile, O_WRONLY|O_CREAT, 0660)) == -1) {
		fprintf(stderr, "Failed to open/create logfile : %s %s\n",
			logfile, strerror(errno));
		exit(1);
	}
	if (dup2(fd, 2) == -1) {
		fprintf(stderr, "Failed to dup2 logfile : %s %s\n",
			logfile, strerror(errno));
		exit(1);
	}
	close(fd);
}

static void LOG(char *fmt, ...)
{
	char *str;
	va_list a_list;

	va_start(a_list, fmt);
	if (vasprintf(&str, fmt, a_list) == -1) {
		_exit(1);
	}
	fprintf(stderr, "%d %s\n", getpid(), str);
	free(str);
}

static void LOG_PDU(char *direction, unsigned char *pdu, int len)
{
	char buf[1024];
	int i, pos;

	for (i = pos = 0; i < len && pos < 1019; i++) {
		if (isprint(pdu[i])) {
			buf[pos++] = pdu[i];
			continue;
		}
		sprintf(buf + pos, "\\%03o", pdu[i]);
		pos += 4;
	}
	buf[pos] = 0;
	LOG("%s PDU Len:%d %s", direction, len, buf);
}

static void setup_databases(char *path)
{
	char tdbfile[PATH_MAX];

	snprintf(tdbfile, PATH_MAX, "%s/%s", path, REFS_TDB);
	refs_tdb = tdb_open(tdbfile, 1000001,
			    TDB_VOLATILE|TDB_ALLOW_NESTING|
			    TDB_INCOMPATIBLE_HASH,
			    O_CREAT|O_RDWR, 0600);
	if (!refs_tdb) {
		LOG("Failed to open %s %s", tdbfile, strerror(errno));
		exit(1);
	}
	snprintf(tdbfile, PATH_MAX, "%s/%s", path, LOGS_TDB);
	logs_tdb = tdb_open(tdbfile, 1000001,
			    TDB_VOLATILE|TDB_ALLOW_NESTING|
			    TDB_INCOMPATIBLE_HASH,
			    O_CREAT|O_RDWR, 0600);
	if (!logs_tdb) {
		LOG("Failed to open %s %s", tdbfile, strerror(errno));
		exit(1);
	}
}

static int create_socket(const char *path)
{
	struct sockaddr_un addr;
	int s;

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));

	if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		LOG("socket already in use: %s", path);
		exit(1);
	}

	unlink(path);

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		LOG("failed to bind to socket: %s %s", path, strerror(errno));
		exit(1);
	}

	if (chown(path, geteuid(), getegid()) != 0 ||
	    chmod(path, 0600) != 0) {
		LOG("failed to set socket permissions: %s %s", path,
		    strerror(errno));
		exit(1);
	}
	return s;
}

static TDB_CONTEXT *get_db(char *db)
{
	if (!strcmp(db, "refs"))
		return refs_tdb;
	else if (!strcmp(db, "logs"))
		return logs_tdb;
	return NULL;
}

static void return_error(int s, char *fmt, ...)
{
	int pos = 0, count, slen;
	uint32_t len;
	struct iovec iov[2];
	char *str;
	va_list a_list;

	va_start(a_list, fmt);
	if (vasprintf(&str, fmt, a_list) == -1) {
		_exit(1);
	}
	slen = strlen(str) + 1;
	len = htonl(slen + 0x80000000);

	LOG("ERROR %s", str);

	while (pos < slen + 4) {
		if (pos < 4) {
			iov[0].iov_base = ((char *)&len) + pos;
			iov[0].iov_len = 4 - pos;
			iov[1].iov_base = str;
			iov[1].iov_len = slen;
		} else {
			iov[0].iov_base = str + pos - 4;
			iov[0].iov_len = slen - pos + 4;
		}
		count = writev(s, iov, pos < 4 ? 2 : 1);
		if (count == -1 && errno == EINTR)
			continue;
		if (count == -1) {
			LOG("lost connection to client: %s", strerror(errno));
			_exit(1);
		}
		pos += count;
	}
	free(str);
}

static void return_success(int s, unsigned char *data, int datalen)
{
	int pos = 0, count;
	uint32_t len;
	struct iovec iov[2];

	LOG_PDU("OK", data, datalen);

	len = htonl(datalen);
	while (pos < datalen + 4) {
		if (pos < 4) {
			iov[0].iov_base = ((char *)&len) + pos;
			iov[0].iov_len = 4 - pos;
			iov[1].iov_base = data;
			iov[1].iov_len = datalen;
		} else {
			iov[0].iov_base = data + pos - 4;
			iov[0].iov_len = datalen - pos + 4;
		}
		count = writev(s, iov, pos < 4 ? 2 : 1);
		if (count == -1 && errno == EINTR)
			continue;
		if (count == -1) {
			LOG("lost connection to client: %s", strerror(errno));
			_exit(1);
		}
		pos += count;
	}
}

static void process_store(int s, unsigned char *buf, int len)
{
	char *db;
	uint32_t keylen;
	TDB_DATA key, data;

	db = buf;
	buf += strlen(db) + 1;
	len -= strlen(db) + 1;
	if (len < 4) {
		char *err = "protocol error. store key length  < 4";
		return_error(s, err);
		_exit(1);
	}

	memcpy(&keylen, buf, sizeof(keylen));
	keylen = ntohl(keylen);
	buf += sizeof(keylen);
	len -= sizeof(keylen);

	if (keylen >= len) {
		char *err = "protocol error. store key length too large";
		return_error(s, err);
		_exit(1);
	}

	key.dptr = buf;
	key.dsize = keylen;
	buf += keylen;
	len -= keylen;

	if (len < 1) {
		char *err = "protocol error. store data length  < 1";
		return_error(s, err);
		_exit(1);
	}

	data.dptr = buf;
	data.dsize = len;

	if (tdb_store(get_db(db), key, data, TDB_REPLACE)) {
		char *str = "tdb_store failed: %s %s";
		return_error(s, str,
			     tdb_name(get_db(db)),
			     tdb_errorstr(get_db(db)));
	} else
		return_success(s, NULL, 0);
}

static void process_fetch(int s, unsigned char *buf, int len)
{
	char *db;
	TDB_DATA key, data;

	db = buf;
	buf += strlen(db) + 1;
	len -= strlen(db) + 1;
	if (len < 0)
		_exit(1);

	key.dptr = buf;
	key.dsize = len;

	if (len < 1) {
		char *err = "protocol error. fetch key length  < 1";
		return_error(s, err);
		_exit(1);
	}

	data = tdb_fetch(get_db(db), key);
	return_success(s, data.dptr, data.dsize);
	free(data.dptr);
}

static void process_delete(int s, unsigned char *buf, int len)
{
	char *db;
	TDB_DATA key;

	db = buf;
	buf += strlen(db) + 1;
	len -= strlen(db) + 1;

	if (len < 1) {
		char *err = "protocol error. deletion key length  < 1";
		return_error(s, err);
		_exit(1);
	}

	key.dptr = buf;
	key.dsize = len;

	if (tdb_delete(get_db(db), key)) {
		char *str = "tdb_delete failed: %s %s";
		return_error(s, str,
			     tdb_name(get_db(db)),
			     tdb_errorstr(get_db(db)));
	} else
		return_success(s, NULL, 0);
}

struct ref_entry_cb {
	const char *repo;
	const char *base;
	unsigned char *buf;
	int bufsize;
};

static int do_one_ref(struct tdb_context *tdb, TDB_DATA key, TDB_DATA data,
		      void *d)
{
	struct ref_entry_cb *cb_data = d;
	unsigned char *pos;
	int flag = 0;
	uint32_t len;
	const char *str = (char *)key.dptr;

	/* First string is the repo name */
	if (strcmp(str, cb_data->repo))
		return 0;

	str += strlen(str) + 1;

	/* Don't iterate over the magic refs, i.e. the onces that live
	   directly under .git
	*/
	if (!strchr((char *)str, '/'))
		return 0;

	if (strncmp((char *)str, cb_data->base, strlen(cb_data->base)))
		return 0;

	cb_data->buf = realloc(cb_data->buf, cb_data->bufsize + 4 + data.dsize +4 + key.dsize);
	if (!cb_data->buf)
		_exit(1);
	pos = cb_data->buf + cb_data->bufsize;

	len = htonl(key.dsize);
	memcpy(pos, &len, sizeof(len));
	pos += 4;

	memcpy(pos, key.dptr, key.dsize);
	pos += key.dsize;

	len = htonl(data.dsize);
	memcpy(pos, &len, sizeof(len));
	pos += 4;

	memcpy(pos, data.dptr, data.dsize);
	pos += data.dsize;

	cb_data->bufsize += 4 + key.dsize + 4 + data.dsize;

	return 0;
}

static void process_traverse(int s, unsigned char *buf, int len)
{
	char *db;
	TDB_DATA key;
	struct ref_entry_cb cb_data;
	int ret;

	cb_data.buf = NULL;
	cb_data.bufsize = 0;

	if (buf[len - 1])
		_exit(1);
	db = buf;
	buf += strlen(db) + 1;
	len -= strlen(db) + 1;
	if (len < 1)
		_exit(1);

	cb_data.repo = buf;
	buf += strlen(cb_data.repo) + 1;
	len -= strlen(cb_data.repo) + 1;
	if (len < 1)
		_exit(1);

	cb_data.base = buf;

	if (tdb_traverse_read(get_db(db), do_one_ref, &cb_data) < 0)
		return_error(s, "tdb_traverse failed: %s %s",
			     tdb_name(get_db(db)),
			     tdb_errorstr(get_db(db)));
	else
		return_success(s, cb_data.buf, cb_data.bufsize);

	free(cb_data.buf);
}

static void process_start(int s)
{
	if (tdb_transaction_start(refs_tdb) == -1) {
		return_error(s, "Failed to start transaction on %s. %s",
			     tdb_name(refs_tdb),
			     tdb_errorstr(refs_tdb));
		return;
	}
	if (tdb_transaction_start(logs_tdb) == -1) {
		return_error(s, "Failed to start transaction on %s. %s",
			     tdb_name(refs_tdb),
			     tdb_errorstr(refs_tdb));
		return;
	}

	return_success(s, NULL, 0);
}

static void process_cancel(int s)
{
	if (tdb_transaction_cancel(refs_tdb) == -1) {
		return_error(s, "Failed to cancel transaction on %s. %s",
			     tdb_name(refs_tdb),
			     tdb_errorstr(refs_tdb));
		return;
	}
	if (tdb_transaction_cancel(logs_tdb) == -1) {
		return_error(s, "Failed to cancel transaction on %s. %s",
			     tdb_name(refs_tdb),
			     tdb_errorstr(refs_tdb));
		return;
	}

	return_success(s, NULL, 0);
}

static void process_commit(int s)
{
	if (tdb_transaction_commit(refs_tdb) == -1) {
		return_error(s, "Failed to commit transaction on %s. %s",
			     tdb_name(refs_tdb),
			     tdb_errorstr(refs_tdb));
		return;
	}
	if (tdb_transaction_commit(logs_tdb) == -1) {
		return_error(s, "Failed to commit transaction on %s. %s",
			     tdb_name(refs_tdb),
			     tdb_errorstr(refs_tdb));
		return;
	}

	return_success(s, NULL, 0);
}

static void process_commands(int s)
{
	uint32_t len;
	int pos = 0, count;
	char *buf;

	while (pos < 4) {
		count = read(s, &len, 4 - pos);
		if (count == -1 && errno == EINTR)
			continue;
		if (count == 0 || count == -1) {
			_exit(1);
		}
		pos += count;
	}
	len = ntohl(len);

	buf = malloc(len);
	if (!buf) {
		LOG("Failed to malloc PDU of size %d", len);
		_exit(1);
	}
	pos = 0;
	while (pos < len) {
		count = read(s, buf + pos, len - pos);
		if (count == -1 && errno == EINTR)
			continue;
		if (count == 0 || count == -1)
			_exit(1);
		pos += count;
	}
	LOG_PDU("RECV", buf, len);

	if (!strcmp(buf, "store"))
		process_store(s, buf + 6, len - 6);
	else if (!strcmp(buf, "fetch"))
		process_fetch(s, buf + 6, len - 6);
	else if (!strcmp(buf, "delete"))
		process_delete(s, buf + 7, len - 7);
	else if (!strcmp(buf, "traverse"))
		process_traverse(s, buf + 9, len - 9);
	else if (!strcmp(buf, "start"))
		process_start(s);
	else if (!strcmp(buf, "cancel"))
		process_cancel(s);
	else if (!strcmp(buf, "commit"))
		process_commit(s);
	else {
		LOG("Unknown command: %s", buf);
		_exit(1);
	}

	free(buf);
}

static void sigchld_handler(int sig)
{
	while (waitpid(-1, 0, WNOHANG) > 0)
		;
}

static void setup_sigchld(void)
{
	struct sigaction sa;

	sa.sa_handler = &sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	if (sigaction(SIGCHLD, &sa, 0) == -1) {
		LOG("failed to register SIGCHLD handler %s", strerror(errno));
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	int s, c;
	struct sockaddr addr;
	socklen_t len;
	pid_t child;

	if (argc != 4)
		usage();

	s = create_socket(argv[1]);
	setup_databases(argv[2]);
	setup_sigchld();

	setup_logging(argv[3]);
	close(0);
	close(1);

	if (daemon(0, 1) == -1) {
		LOG("daemon() failed with %s", strerror(errno));
	}

	if (listen(s, 10) == -1) {
		LOG("failed to listen to socket: %s", strerror(errno));
		exit(1);
	}

	while (1) {
		if ((c = accept(s, &addr, &len)) == -1) {
			LOG("failed to accept socket: %s", strerror(errno));
			exit(1);
		}
		child = fork();
		if (child == -1) {
			LOG("fork failed: %s", strerror(errno));
			exit(1);
		}
		if (!child) {
			close(s);
			while(1)
				process_commands(c);
			_exit(0);
		}
		close(c);
	}

	return 0;
}
