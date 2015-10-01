/*
 * Copyright (C) 2011 John Szakmeister <john@szakmeister.net>
 *               2012 Philipp A. Hartmann <pah@qo.cx>
 *               2015 Philipp Edelmann <edelmann@fs.tum.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Credits:
 * - GNOME Keyring API handling originally written by John Szakmeister
 * - ported to credential helper API by Philipp A. Hartmann
 * - ported to libsecret by Philipp Edelmann
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <glib.h>
#include <libsecret/secret.h>
#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr-base.h>


/*
 * This credential struct and API is simplified from git's credential.{h,c}
 */
struct credential {
	char *protocol;
	char *host;
	unsigned short port;
	char *path;
	char *username;
	char *password;
};

#define CREDENTIAL_INIT { NULL, NULL, 0, NULL, NULL, NULL }

typedef int (*credential_op_cb)(struct credential *);

struct credential_operation {
	char *name;
	credential_op_cb op;
};

#define CREDENTIAL_OP_END { NULL, NULL }

/* create a special keyring option string, if path is given */
static char *keyring_object(struct credential *c)
{
	if (!c->path)
		return NULL;

	if (c->port)
		return g_strdup_printf("%s:%hd/%s", c->host, c->port, c->path);

	return g_strdup_printf("%s/%s", c->host, c->path);
}

static int keyring_get(struct credential *c)
{
	char *object = NULL;
	gchar *password_data = NULL;
	GError *error = NULL;

	if (!c->protocol || !(c->host || c->path))
		return EXIT_FAILURE;

	object = keyring_object(c);

	password_data = secret_password_lookup_nonpageable_sync(
			SECRET_SCHEMA_COMPAT_NETWORK,
			NULL,
			&error,
			"server",
			c->host,
			"user",
			c->username,
			"protocol",
			c->protocol,
			"port",
			c->port,
			object ? "object" : NULL,
			object ?  object  : NULL,
			NULL);

	g_free(object);

	if (error) {
		g_critical("%s", error->message);
		g_error_free(error);
	}

	if (password_data == NULL) {
		g_critical("not found!");
		return EXIT_FAILURE;
	}

	gcr_secure_memory_free(c->password);
	c->password = gcr_secure_memory_strdup(password_data);
	secret_password_free(password_data);

	return EXIT_SUCCESS;
}


static int keyring_store(struct credential *c)
{
	char *object = NULL;
	gboolean result;
	GError *error = NULL;

	/*
	 * Sanity check that what we are storing is actually sensible.
	 * In particular, we can't make a URL without a protocol field.
	 * Without either a host or pathname (depending on the scheme),
	 * we have no primary key. And without a username and password,
	 * we are not actually storing a credential.
	 */
	if (!c->protocol || !(c->host || c->path) ||
	    !c->username || !c->password)
		return EXIT_FAILURE;

	object = keyring_object(c);

	result = secret_password_store_sync(
			SECRET_SCHEMA_COMPAT_NETWORK,
			SECRET_COLLECTION_DEFAULT,
			c->host,
			c->password,
			NULL,
			&error,
			"server",
			c->host,
			"user",
			c->username,
			"protocol",
			c->protocol,
			"port",
			c->port,
			object ? "object" : NULL,
			object ?  object  : NULL,
			NULL);

	g_free(object);

	if (error) {
		g_critical("%s", error->message);
		g_error_free(error);
	}

	if (!result) {
		if (error) {
			g_critical("%s", error->message);
			g_error_free(error);
		}
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static int keyring_erase(struct credential *c)
{
	char *object = NULL;
	gboolean result;
	GError *error = NULL;

	/*
	 * Sanity check that we actually have something to match
	 * against. The input we get is a restrictive pattern,
	 * so technically a blank credential means "erase everything".
	 * But it is too easy to accidentally send this, since it is equivalent
	 * to empty input. So explicitly disallow it, and require that the
	 * pattern have some actual content to match.
	 */
	if (!c->protocol && !c->host && !c->path && !c->username)
		return EXIT_FAILURE;

	object = keyring_object(c);

	result = secret_password_clear_sync(
			SECRET_SCHEMA_COMPAT_NETWORK,
			NULL,
			&error,
			"server",
			c->host,
			"user",
			c->username,
			"protocol",
			c->protocol,
			"port",
			c->port,
			object ? "object" : NULL,
			object ?  object  : NULL,
			NULL);

	g_free(object);

	if (error) {
		g_critical("%s", error->message);
		g_error_free(error);
	}

	if (result)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}

/*
 * Table with helper operation callbacks, used by generic
 * credential helper main function.
 */
static struct credential_operation const credential_helper_ops[] = {
	{ "get",   keyring_get },
	{ "store", keyring_store },
	{ "erase", keyring_erase },
	CREDENTIAL_OP_END
};

/* ------------------ credential functions ------------------ */

static void credential_init(struct credential *c)
{
	memset(c, 0, sizeof(*c));
}

static void credential_clear(struct credential *c)
{
	g_free(c->protocol);
	g_free(c->host);
	g_free(c->path);
	g_free(c->username);
	gcr_secure_memory_free(c->password);

	credential_init(c);
}

static int credential_read(struct credential *c)
{
	char *buf;
	size_t line_len;
	char *key;
	char *value;

	key = buf = gcr_secure_memory_new(char, 1024);

	while (fgets(buf, 1024, stdin)) {
		line_len = strlen(buf);

		if (line_len && buf[line_len-1] == '\n')
			buf[--line_len] = '\0';

		if (!line_len)
			break;

		value = strchr(buf, '=');
		if (!value) {
			g_warning("invalid credential line: %s", key);
			gcr_secure_memory_free(buf);
			return -1;
		}
		*value++ = '\0';

		if (!strcmp(key, "protocol")) {
			g_free(c->protocol);
			c->protocol = g_strdup(value);
		} else if (!strcmp(key, "host")) {
			g_free(c->host);
			c->host = g_strdup(value);
			value = strrchr(c->host, ':');
			if (value) {
				*value++ = '\0';
				c->port = atoi(value);
			}
		} else if (!strcmp(key, "path")) {
			g_free(c->path);
			c->path = g_strdup(value);
		} else if (!strcmp(key, "username")) {
			g_free(c->username);
			c->username = g_strdup(value);
		} else if (!strcmp(key, "password")) {
			gcr_secure_memory_free(c->password);
			c->password = gcr_secure_memory_strdup(value);
			while (*value)
				*value++ = '\0';
		}
		/*
		 * Ignore other lines; we don't know what they mean, but
		 * this future-proofs us when later versions of git do
		 * learn new lines, and the helpers are updated to match.
		 */
	}

	gcr_secure_memory_free(buf);

	return 0;
}

static void credential_write_item(FILE *fp, const char *key, const char *value)
{
	if (!value)
		return;
	fprintf(fp, "%s=%s\n", key, value);
}

static void credential_write(const struct credential *c)
{
	/* only write username/password, if set */
	credential_write_item(stdout, "username", c->username);
	credential_write_item(stdout, "password", c->password);
}

static void usage(const char *name)
{
	struct credential_operation const *try_op = credential_helper_ops;
	const char *basename = strrchr(name, '/');

	basename = (basename) ? basename + 1 : name;
	fprintf(stderr, "usage: %s <", basename);
	while (try_op->name) {
		fprintf(stderr, "%s", (try_op++)->name);
		if (try_op->name)
			fprintf(stderr, "%s", "|");
	}
	fprintf(stderr, "%s", ">\n");
}

int main(int argc, char *argv[])
{
	int ret = EXIT_SUCCESS;

	struct credential_operation const *try_op = credential_helper_ops;
	struct credential cred = CREDENTIAL_INIT;

	if (!argv[1]) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	g_set_application_name("Git Credential Helper");

	/* lookup operation callback */
	while (try_op->name && strcmp(argv[1], try_op->name))
		try_op++;

	/* unsupported operation given -- ignore silently */
	if (!try_op->name || !try_op->op)
		goto out;

	ret = credential_read(&cred);
	if (ret)
		goto out;

	/* perform credential operation */
	ret = (*try_op->op)(&cred);

	credential_write(&cred);

out:
	credential_clear(&cred);
	return ret;
}
