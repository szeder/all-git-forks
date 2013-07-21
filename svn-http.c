#include "remote-svn.h"
#include "cache.h"
#include "http.h"
#include "credential.h"
#include "object.h"
#include <expat.h>

/* url does not have a trailing slash, includes rootpath */
static struct strbuf url = STRBUF_INIT;
static int pathoff;

static struct strbuf cmt_work_path = STRBUF_INIT;
static struct strbuf cmt_activity = STRBUF_INIT;
static int cmt_mkactivity;

struct request {
	XML_Parser parser;
	struct active_request_slot *slot;
	struct slot_results res;
	struct buffer in;
	struct strbuf url;
	struct strbuf header;
	struct strbuf cdata;
	struct curl_slist *hdrs;
	const char *method;
	curl_write_callback hdrfunc;
	void *callback_data;
	void (*callback_func)(void *data);
	unsigned int just_opened : 1;
};

static struct request main_request;

static void append_path(struct strbuf *buf, const char *p, int sz) {
	if (sz < 0)
		sz = (int) strlen(p);

	while (sz-- > 0) {
		if ('a' <= *p && *p <= 'z') {
			strbuf_addch(buf, *p);
		} else if ('A' <= *p && *p <= 'Z') {
			strbuf_addch(buf, *p);
		} else if ('0' <= *p && *p <= '9') {
			strbuf_addch(buf, *p);
		} else if (*p == '/' || *p == '-' || *p == '_' || *p == '.' || *p == '(' || *p == ')' || *p == '[' || *p == ']' || *p == ',') {
			strbuf_addch(buf, *p);
		} else {
			strbuf_addf(buf, "%%%02X", *(unsigned char*)p);
		}

		p++;
	}
}

static void encode_xml(struct strbuf *buf, const char *p) {
	while (*p) {
		switch (*p) {
		case '"':
			strbuf_addstr(buf, "&quot;");
			break;
		case '\'':
			strbuf_addstr(buf, "&apos;");
			break;
		case '<':
			strbuf_addstr(buf, "&lt;");
			break;
		case '>':
			strbuf_addstr(buf, "&gt;");
			break;
		case '&':
			strbuf_addstr(buf, "&amp;");
			break;
		default:
			strbuf_addch(buf, *p);
			break;
		}

		p++;
	}
}

static const char* shorten_tag(const char* name) {
	static struct strbuf buf = STRBUF_INIT;
	strbuf_reset(&buf);

	if (!prefixcmp(name, "svn:|")) {
		strbuf_addf(&buf, "S:%s", name + strlen("svn:|"));
	} else if (!prefixcmp(name, "DAV:|")) {
		strbuf_addf(&buf, "D:%s", name + strlen("DAV:|"));
	} else if (!prefixcmp(name, "http://subversion.tigris.org/xmlns/dav/|")) {
		strbuf_addf(&buf, "V:%s", name + strlen("http://subversion.tigris.org/xmlns/dav/|"));
	}

	return buf.len ? buf.buf : name;
}

static void xml_start(void *user, const char *name, const char **attrs) {
	struct request *h = user;

	strbuf_reset(&h->cdata);

	if (svndbg >= 2) {
		if (h->just_opened) {
			fprintf(stderr, ">\n");
		}
		fprintf(stderr, "%s<%s", h->header.buf, shorten_tag(name));
		while (attrs[0] && attrs[1]) {
			fprintf(stderr, " %s=\"%s\"", attrs[0], attrs[1]);
			attrs += 2;
		}
		strbuf_addch(&h->header, ' ');
		h->just_opened = 1;
	}
}

static void xml_end(void *user, const char *name, int limitdbg) {
	struct request *h = user;
	strbuf_trim(&h->cdata);

	if (svndbg >= 2) {
		strbuf_setlen(&h->header, h->header.len - 1);

		if (h->just_opened && !h->cdata.len) {
			fprintf(stderr, "/>\n");
		} else if (h->just_opened) {
			fprintf(stderr, ">");
		} else {
			fprintf(stderr, "%s", h->header.buf);
		}

		if (limitdbg && h->cdata.len > 20) {
			fprintf(stderr, "%.*s...</%s>\n", 64, h->cdata.buf, shorten_tag(name));
		} else if (h->cdata.len || !h->just_opened) {
			fprintf(stderr, "%s</%s>\n", h->cdata.buf, shorten_tag(name));
		}

		h->just_opened = 0;
	}
}

static size_t write_xml(char *ptr, size_t eltsize, size_t sz, void *report_) {
	struct request *h = report_;
	sz *= eltsize;
	if (h && !XML_Parse(h->parser, ptr, sz, 0)) {
		die("xml parse error %.*s", (int) sz, ptr);
	}
	return sz;
}

static void xml_cdata_cb(void *user, const XML_Char *s, int len) {
	struct request *h = user;
	strbuf_add(&h->cdata, s, len);
}

static void process_request(struct request *h, XML_StartElementHandler start, XML_EndElementHandler end) {
	strbuf_release(&h->cdata);
	strbuf_reset(&h->header);
	XML_SetCharacterDataHandler(h->parser, &xml_cdata_cb);
	XML_SetElementHandler(h->parser, start, end);
	XML_SetUserData(h->parser, h);
}

static void init_request(struct request *h) {
	memset(h, 0, sizeof(*h));
	h->parser = XML_ParserCreateNS("UTF-8", '|');
	strbuf_init(&h->in.buf, 0);
	strbuf_init(&h->header, 0);
	strbuf_init(&h->cdata, 0);
	strbuf_init(&h->url, 0);
}

static void reset_request(struct request *h) {
	strbuf_reset(&h->url);
	strbuf_add(&h->url, url.buf, url.len);
	strbuf_reset(&h->in.buf);
	h->hdrs = NULL;
	h->hdrfunc = NULL;
	h->method = NULL;
	XML_ParserReset(h->parser, "UTF-8");
	h->callback_func = NULL;
	h->callback_data = NULL;
}

static void start_request(struct request *h) {
	static struct curl_slist *defhdrs;
	CURL *c;

	if (!defhdrs) {
		defhdrs = curl_slist_append(defhdrs, "Expect:");
		defhdrs = curl_slist_append(defhdrs, "DAV: http://subversion.tigris.org/xmlns/dav/svn/depth");
		defhdrs = curl_slist_append(defhdrs, "Pragma: no-cache");
	}

	h->in.posn = 0;
	h->just_opened = 0;
	h->slot = get_active_slot();
	h->slot->results = &h->res;
	h->slot->callback_func = h->callback_func;
	h->slot->callback_data = h->callback_data;

	c = h->slot->curl;
	curl_easy_setopt(c, CURLOPT_PUT, 1);
	curl_easy_setopt(c, CURLOPT_NOBODY, 0);
	curl_easy_setopt(c, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(c, CURLOPT_HTTPHEADER, h->hdrs ? h->hdrs : defhdrs);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_xml);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, XML_GetUserData(h->parser));
	curl_easy_setopt(c, CURLOPT_HEADERFUNCTION, h->hdrfunc);
	curl_easy_setopt(c, CURLOPT_CUSTOMREQUEST, h->method);
	curl_easy_setopt(c, CURLOPT_URL, h->url.buf);
	curl_easy_setopt(c, CURLOPT_READFUNCTION, fread_buffer);
	curl_easy_setopt(c, CURLOPT_INFILE, &h->in);
	curl_easy_setopt(c, CURLOPT_INFILESIZE, h->in.buf.len);
	curl_easy_setopt(c, CURLOPT_IOCTLFUNCTION, ioctl_buffer);
	curl_easy_setopt(c, CURLOPT_IOCTLDATA, &h->in);
	curl_easy_setopt(c, CURLOPT_ENCODING, "svndiff1;q=0.9,svndiff;q=0.8");

	if (svndbg >= 2)
		fprintf(stderr, "start report %s\n%s\n", h->url.buf, h->in.buf.buf);

	if (!start_active_slot(h->slot)) {
		die("request-log failed %d\n", (int) h->res.http_code);
	}
}

static int run_request(struct request *h) {
	int ret;
	start_request(h);
	run_active_slot(h->slot);
	ret = handle_curl_result(h->slot);

	if (ret == HTTP_REAUTH) {
		start_request(h);
		run_active_slot(h->slot);
		ret = handle_curl_result(h->slot);
	}

	if (ret) {
		/* We don't use http_request so we want http_error to
		 * log in all instances so feed a fake error to avoid
		 * the check on HTTP_START_FAILED */
		http_error(h->url.buf, HTTP_ERROR);
	}

	return ret;
}

static int get_header(struct strbuf* buf, const char* hdr, char* ptr, size_t size) {
	size_t hsz = strlen(hdr);
	if (size < hsz || memcmp(ptr, hdr, hsz))
		return 0;

	strbuf_reset(buf);
	strbuf_add(buf, ptr + hsz, size - hsz);
	strbuf_trim(buf);
	return 1;
}

static int latest_rev = -1;
static struct strbuf *uuid;

static size_t options_header(char *ptr, size_t size, size_t nmemb, void *userdata) {
	struct strbuf buf = STRBUF_INIT;

	size *= nmemb;

	if (get_header(&buf, "SVN-Youngest-Rev: ", ptr, size)) {
		latest_rev = atoi(buf.buf);

	} else if (get_header(&buf, "SVN-Repository-Root: ", ptr, size)) {
		clean_svn_path(&buf);
		strbuf_setlen(&url, pathoff + buf.len);

	} else if (uuid && get_header(uuid, "SVN-Repository-UUID: ", ptr, size)) {
	}

	strbuf_release(&buf);
	return size;
}

static struct strbuf latest_href = STRBUF_INIT;

static void latest_xml_start(void *user, const char *name, const char **attrs) {
	xml_start(user, name, attrs);

	if (!strcmp(name, "DAV:|href")) {
		strbuf_reset(&latest_href);
	}
}

static void latest_xml_end(void *user, const char *name) {
	struct request *h = user;
	xml_end(h, name, 0);

	if (!strcmp(name, "DAV:|href")) {
		strbuf_swap(&latest_href, &h->cdata);

	} else if (!strcmp(name, "DAV:|version-name")) {
		latest_rev = atoi(h->cdata.buf);

	} else if (!strcmp(name, "http://subversion.tigris.org/xmlns/dav/|baseline-relative-path")) {
		clean_svn_path(&h->cdata);
		strbuf_setlen(&url, url.len - h->cdata.len);

	} else if (uuid && !strcmp(name, "http://subversion.tigris.org/xmlns/dav/|repository-uuid")) {
		strbuf_swap(uuid, &h->cdata);

	}

	strbuf_reset(&h->cdata);
}

static int http_get_latest(void) {
	return latest_rev;
}

static void http_get_options(void) {
	struct request *h = &main_request;

	reset_request(h);
	h->hdrfunc = &options_header;
	h->method = "OPTIONS";

	strbuf_addstr(&h->in.buf,
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<D:options xmlns:D=\"DAV:\">\n"
		" <D:activity-collection-set/>\n"
		"</D:options>\n");

	if (run_request(h))
		goto err;

	/* Pre 1.7 doesn't send the uuid/version-number as OPTION headers */
	if (latest_rev < 0) {
		reset_request(h);
		h->method = "PROPFIND";
		h->hdrs = curl_slist_append(NULL, "Expect:");
		h->hdrs = curl_slist_append(h->hdrs, "Depth: 0");

		strbuf_addstr(&h->in.buf,
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			"<D:propfind xmlns:D=\"DAV:\" xmlns:S=\"http://subversion.tigris.org/xmlns/dav/\">\n"
			" <D:prop>\n"
			"  <S:baseline-relative-path/>\n"
			"  <S:repository-uuid/>\n"
			"  <D:version-name/>\n"
			" </D:prop>\n"
			"</D:propfind>\n");

		process_request(h, &latest_xml_start, &latest_xml_end);
		if (run_request(h))
			goto err;

		curl_slist_free_all(h->hdrs);

		cmt_mkactivity = 1;
	}

	return;

err:
	die("get_latest failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
}

static int list_collection, list_off;
static struct string_list *list_dirs;
static char *list_href;

static void list_xml_end(void *user, const char *name) {
	struct request *h = user;

	xml_end(h, name, 0);

	if (!strcmp(name, "DAV:|collection")) {
		list_collection = 1;

	} else if (!strcmp(name, "DAV:|href") && h->cdata.len >= list_off) {
		strbuf_remove(&h->cdata, 0, list_off);
		clean_svn_path(&h->cdata);
		free(list_href);
		list_href = url_decode_mem(h->cdata.buf, h->cdata.len);
		if (!list_href)
			list_href = strdup("");

	} else if (!strcmp(name, "DAV:|response")) {
		if (list_collection && list_href) {
			string_list_insert(list_dirs, list_href);
		}

		free(list_href);
		list_href = NULL;
		list_collection = 0;
	}

	strbuf_reset(&h->cdata);
}

static void do_list(const char *path, int rev, struct string_list *dirs, struct curl_slist *hdrs) {
	struct request *h = &main_request;

	reset_request(h);
	h->hdrs = hdrs;
	h->method = "PROPFIND";

	strbuf_addf(&h->url, "/!svn/bc/%d", rev);
	append_path(&h->url, path, -1);

	strbuf_addstr(&h->in.buf,
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<propfind xmlns=\"DAV:\">\n"
		" <prop><resourcetype/></prop>\n"
		"</propfind>\n");

	list_dirs = dirs;
	list_off = h->url.len - pathoff;
	list_collection = 0;

	process_request(h, &xml_start, &list_xml_end);

	free(list_href);
	list_href = NULL;

	if (run_request(h) && h->res.http_code != 404) {
		die("propfind failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
	}
}

static void http_list(const char *path, int rev, struct string_list *dirs) {
	static struct curl_slist *hdrs;
	if (!hdrs) {
		hdrs = curl_slist_append(hdrs, "Expect:");
		hdrs = curl_slist_append(hdrs, "Depth: 1");
	}
	do_list(path, rev, dirs, hdrs);
}

static int http_isdir(const char *path, int rev) {
	static struct curl_slist *hdrs;
	struct string_list dirs = STRING_LIST_INIT_NODUP;
	int ret;

	if (!hdrs) {
		hdrs = curl_slist_append(hdrs, "Expect:");
		hdrs = curl_slist_append(hdrs, "Depth: 0");
	}

	do_list(path, rev, &dirs, hdrs);

	ret = dirs.nr != 0;
	string_list_clear(&dirs, 0);
	return ret;
}

static struct mergeinfo *get_mergeinfo;

static void get_mergeinfo_xml_end(void *user, const char *name) {
	struct request *h = user;

	xml_end(h, name, 0);
	if (!strcmp(name, "http://subversion.tigris.org/xmlns/svn/|mergeinfo") && !get_mergeinfo) {
		get_mergeinfo = parse_svn_mergeinfo(h->cdata.buf);
	}
	strbuf_reset(&h->cdata);
}

static struct mergeinfo *http_get_mergeinfo(const char *path, int rev) {
	static struct curl_slist *hdrs;
	struct request *h = &main_request;

	if (!hdrs) {
		hdrs = curl_slist_append(hdrs, "Expect:");
		hdrs = curl_slist_append(hdrs, "Depth: 0");
	}

	reset_request(h);
	h->hdrs = hdrs;
	h->method = "PROPFIND";

	strbuf_addf(&h->url, "/!svn/rvr/%d", rev);
	append_path(&h->url, path, -1);

	strbuf_addstr(&h->in.buf,
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		"<propfind xmlns=\"DAV:\">\n"
		" <prop><mergeinfo xmlns=\"http://subversion.tigris.org/xmlns/svn/\"/></prop>\n"
		"</propfind>\n");

	get_mergeinfo = NULL;
	process_request(h, &xml_start, &get_mergeinfo_xml_end);
	if (run_request(h) || !get_mergeinfo) {
		die("mergeinfo propfind failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
	}

	return get_mergeinfo;
}







static struct svnref **log_refs;
static int log_refnr;

static struct strbuf log_msg = STRBUF_INIT;
static struct strbuf log_author = STRBUF_INIT;
static struct strbuf log_time = STRBUF_INIT;
static struct strbuf log_copy = STRBUF_INIT;
static int log_rev, log_copyrev;

static void log_xml_start(void *user, const XML_Char *name, const XML_Char **attrs) {
	struct request *h = user;

	xml_start(h, name, attrs);

	if (!strcmp(name, "svn:|log-item")) {
		strbuf_reset(&log_msg);
		strbuf_reset(&log_author);
		strbuf_reset(&log_time);
		log_rev = 0;

	} else if (!strcmp(name, "svn:|added-path")
			|| !strcmp(name, "svn:|replaced-path")
			|| !strcmp(name, "svn:|deleted-path")
			|| !strcmp(name, "svn:|modified-path"))
	{
		log_copyrev = 0;
		strbuf_reset(&log_copy);

		while (attrs[0] && attrs[1]) {
			const char *key = *(attrs++);
			const char *val = *(attrs++);

			if (!strcmp(key, "copyfrom-path")) {
				strbuf_addstr(&log_copy, val);
				clean_svn_path(&log_copy);
			} else if (!strcmp(key, "copyfrom-rev")) {
				log_copyrev = atoi(val);
			}
		}
	}
}

static void log_xml_end(void *user, const XML_Char *name) {
	struct request *h = user;

	xml_end(h, name, 0);

	if (!strcmp(name, "svn:|log-item")) {
		cmt_read(log_refs, log_refnr, log_rev, log_author.buf, log_time.buf, log_msg.buf);

	} else if (!strcmp(name, "DAV:|version-name")) {
		log_rev = atoi(h->cdata.buf);

	} else if (!strcmp(name, "DAV:|comment")) {
		strbuf_swap(&h->cdata, &log_msg);

	} else if (!strcmp(name, "DAV:|creator-displayname")) {
		strbuf_swap(&h->cdata, &log_author);

	} else if (!strcmp(name, "svn:|date")) {
		strbuf_swap(&h->cdata, &log_time);

	} else if (!strcmp(name, "svn:|modified-path")) {
		clean_svn_path(&h->cdata);
		changed_path_read(log_refs, log_refnr, 1, h->cdata.buf, log_copy.buf, log_copyrev);

	} else if (!strcmp(name, "svn:|replaced-path")
			|| !strcmp(name, "svn:|deleted-path")
			|| !strcmp(name, "svn:|added-path"))
	{
		clean_svn_path(&h->cdata);
		changed_path_read(log_refs, log_refnr, 0, h->cdata.buf, log_copy.buf, log_copyrev);
	}

	strbuf_reset(&h->cdata);
}

static void http_read_log(struct svnref **refs, int refnr, int start, int end) {
	struct request *h = &main_request;
	struct strbuf *b = &h->in.buf;
	int i, path_common = strlen(refs[0]->path);

	for (i = 1; i < refnr; i++) {
		const char *path = refs[i]->path;
		path_common = common_directory(refs[0]->path, path, path_common, NULL);
	}

	reset_request(h);
	h->method = "REPORT";

	strbuf_addf(&h->url, "/!svn/ver/%d", end);
	append_path(&h->url, refs[0]->path, path_common);

	strbuf_addstr(b, "<S:log-report xmlns:S=\"svn:\">\n");
	strbuf_addstr(b, " <S:strict-node-history/>\n");
	strbuf_addf(b, " <S:start-revision>%d</S:start-revision>\n", end);
	strbuf_addf(b, " <S:end-revision>%d</S:end-revision>\n", start);
	strbuf_addstr(b, " <S:discover-changed-paths/>\n");
	strbuf_addstr(b, " <S:revprop>svn:author</S:revprop>\n");
	strbuf_addstr(b, " <S:revprop>svn:date</S:revprop>\n");
	strbuf_addstr(b, " <S:revprop>svn:log</S:revprop>\n");

	for (i = 0; i < refnr; i++) {
		strbuf_addstr(b, " <S:path>");
		encode_xml(b, refs[i]->path + path_common);
		strbuf_addstr(b, "</S:path>\n");
	}
	strbuf_addstr(b, "</S:log-report>\n");

	log_refs = refs;
	log_refnr = refnr;

	process_request(h, &log_xml_start, &log_xml_end);
	if (run_request(h) && h->res.http_code) {
		die("log failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
	}
}






struct update {
	struct request req;
	struct strbuf path, diff, hash;
	struct svn_entry *cmt;
	struct update *next;
};

static void add_name(struct strbuf *buf, const XML_Char **p) {
	while (p[0] && p[1]) {
		if (!strcmp(p[0], "name")) {
			strbuf_addch(buf, '/');
			strbuf_addstr(buf, p[1]);
			clean_svn_path(buf);
			return;
		}
		p += 2;
	}
}

static void update_xml_start(void *user, const XML_Char *name, const XML_Char **attrs) {
	struct update *u = user;
	struct request *h = &u->req;

	xml_start(h, name, attrs);

	if (!strcmp(name, "svn:|open-directory")
			|| !strcmp(name, "svn:|add-file")
			|| !strcmp(name, "svn:|open-file"))
	{
		add_name(&u->path, attrs);

	} else if (!strcmp(name, "svn:|add-directory")) {
		add_name(&u->path, attrs);
		helperf(u->cmt, "add-dir %d:%s\n", (int) u->path.len, u->path.buf);

	} else if (!strcmp(name, "svn:|delete-entry")) {
		add_name(&u->path, attrs);
		helperf(u->cmt, "delete-entry %d:%s\n", (int) u->path.len, u->path.buf);
	}
}

static void update_xml_end(void *user, const XML_Char *name) {
	struct update *u = user;
	struct request *h = &u->req;
	char *p;

	xml_end(h, name, !strcmp(name, "svn:|txdelta"));

	if (!strcmp(name, "svn:|txdelta") && h->cdata.len) {
		decode_64(&h->cdata);
		strbuf_swap(&h->cdata, &u->diff);

	} else if (!strcmp(name, "http://subversion.tigris.org/xmlns/dav/|md5-checksum")) {
		strbuf_swap(&u->hash, &h->cdata);

	} else if (!strcmp(name, "svn:|add-directory")
		|| !strcmp(name, "svn:|open-directory")
		|| !strcmp(name, "svn:|delete-entry"))
	{
		p = strrchr(u->path.buf, '/');
		if (p) strbuf_setlen(&u->path, p - u->path.buf);

	} else if (!strcmp(name, "svn:|add-file") || !strcmp(name, "svn:|open-file")) {
		if (u->diff.len) {
			/*add/open-file path before after diff */
			helperf(u->cmt, "%s %d:%s 0: %d:%s %d:",
					name + strlen("svn:|"),
					(int) u->path.len, u->path.buf,
					(int) u->hash.len, u->hash.buf,
					(int) u->diff.len);

			write_helper(u->cmt, u->diff.buf, u->diff.len, 1);
		}

		strbuf_reset(&u->hash);
		strbuf_reset(&u->diff);

		p = strrchr(u->path.buf, '/');
		if (p) strbuf_setlen(&u->path, p - u->path.buf);
	}

	strbuf_reset(&h->cdata);
}

static struct update *free_update;

static void update_finished(void *user) {
	struct update *u = user;
	struct request *h = &u->req;
	int ret = handle_curl_result(h->slot);

	switch (ret) {
	case HTTP_REAUTH:
		start_request(h);
		break;

	case HTTP_OK:
		svn_finish_update(u->cmt);
		u->next = free_update;
		free_update = u;
		break;

	default:
		http_error(h->url.buf, ret);
		die("update failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
		break;
	}
}

static int fill_read_updates(void *user) {
	struct svn_entry *c;
	struct update *u;
	struct request *h;
	struct strbuf *b;

	c = svn_start_next_update();
	if (!c)
		return 0;

	if (free_update) {
		u = free_update;
		free_update = u->next;
	} else {
		u = xcalloc(1, sizeof(*u));
		init_request(&u->req);
		strbuf_init(&u->path, 0);
		strbuf_init(&u->diff, 0);
		strbuf_init(&u->hash, 0);
	}

	u->cmt = c;
	h = &u->req;

	reset_request(h);
	h->method = "REPORT";
	strbuf_addstr(&h->url, "/!svn/vcc/default");

	b = &h->in.buf;
	strbuf_addstr(b, "<S:update-report send-all=\"true\" xmlns:S=\"svn:\">\n");

	strbuf_addstr(b, " <S:src-path>");
	strbuf_add(b, url.buf, url.len);
	encode_xml(b, c->copysrc ? c->copysrc : c->ref->path);
	strbuf_addstr(b, "</S:src-path>\n");

	strbuf_addstr(b, " <S:dst-path>");
	strbuf_add(b, url.buf, url.len);
	encode_xml(b, c->ref->path);
	strbuf_addstr(b, "</S:dst-path>\n");

	strbuf_addf(b, " <S:target-revision>%d</S:target-revision>\n", c->rev);
	strbuf_addstr(b, " <S:depth>unknown</S:depth>\n");
	strbuf_addstr(b, " <S:ignore-ancestry>yes</S:ignore-ancestry>\n");

	if (c->copysrc) {
		strbuf_addf(b, " <S:entry rev=\"%d\" depth=\"infinity\"/>\n", c->copyrev);
	} else if (c->new_branch) {
		strbuf_addf(b, " <S:entry rev=\"%d\" depth=\"infinity\" start-empty=\"true\"/>\n", c->rev);
	} else {
		strbuf_addf(b, " <S:entry rev=\"%d\" depth=\"infinity\"/>\n", c->prev);
	}

	strbuf_addstr(b, "</S:update-report>\n");

	process_request(h, &update_xml_start, &update_xml_end);

	h->callback_func = &update_finished;
	h->callback_data = u;

	start_request(h);
	return 1;
}

static void http_read_updates(int cmts) {
	(void) cmts;
	add_fill_function(NULL, &fill_read_updates);
	fill_active_slots();
	finish_all_active_slots();
	remove_fill_function(NULL, &fill_read_updates);
}







static size_t create_commit_header(char *ptr, size_t size, size_t nmemb, void *userdata) {
	size *= nmemb;

	if (get_header(&cmt_activity, "SVN-Txn-Name: ", ptr, size)) {
	}

	return size;
}

static struct strbuf *location_header;
static size_t get_location_header(char *ptr, size_t size, size_t nmemb, void *userdata) {
	size *= nmemb;

	if (get_header(location_header, "Location: ", ptr, size)) {
		if (prefixcmp(location_header->buf, url.buf)) {
			die("returned location %s points to a different url than %s", location_header->buf, url.buf);
		}
		strbuf_remove(location_header, 0, url.len);
		clean_svn_path(location_header);
	}

	return size;
}

static void http_checkout(struct request *h, struct strbuf *dst) {
	struct strbuf *b;

	h->method = "CHECKOUT";
	h->hdrfunc = &get_location_header;

	b = &h->in.buf;
	strbuf_addstr(b, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	strbuf_addstr(b, "<D:checkout xmlns:D=\"DAV:\">\n");
	strbuf_addf(b, " <D:activity-set><D:href>%s/!svn/act/%s</D:href></D:activity-set>\n", url.buf + pathoff, cmt_activity.buf);
	strbuf_addstr(b, " <D:apply-to-version/>\n");
	strbuf_addstr(b, "</D:checkout>\n");

	location_header = dst;

	if (run_request(h))
		die("checkout failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
}

static void http_delete(const char *svnpath) {
	struct request *h = &main_request;
	reset_request(h);
	h->method = "DELETE";
	strbuf_addstr(&h->url, cmt_work_path.buf);
	append_path(&h->url, svnpath, -1);
	if (run_request(h))
		die("delete failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
}

static void http_mkdir(const char *svnpath) {
	struct request *h = &main_request;
	reset_request(h);
	h->method = "MKCOL";
	strbuf_addstr(&h->url, cmt_work_path.buf);
	append_path(&h->url, svnpath, -1);
	if (run_request(h))
		die("mkcol failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
}

static struct curl_slist *add_file_hdrs, *open_file_hdrs;

static void http_send_file(const char *svnpath, struct strbuf *data, int create) {
	struct request *h = &main_request;

	reset_request(h);
	h->hdrs = create ? add_file_hdrs : open_file_hdrs;
	h->method = "PUT";

	strbuf_addstr(&h->url, cmt_work_path.buf);
	append_path(&h->url, svnpath, -1);
	strbuf_swap(&h->in.buf, data);

	if (run_request(h))
		die("put failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
}

static void http_set_mergeinfo(const char *path, struct mergeinfo *mi) {
	struct request *h = &main_request;
	struct strbuf *b = &h->in.buf;
	reset_request(h);

	h->method = "PROPPATCH";

	strbuf_addstr(&h->url, cmt_work_path.buf);
	append_path(&h->url, path, -1);

	strbuf_addstr(b, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	strbuf_addstr(b, "<D:propertyupdate xmlns:D=\"DAV:\" xmlns:S=\"http://subversion.tigris.org/xmlns/svn/\">\n");
	strbuf_addstr(b, " <D:set><D:prop><S:mergeinfo>");
	encode_xml(b, make_svn_mergeinfo(mi));
	strbuf_addstr(b, "</S:mergeinfo></D:prop></D:set>\n");
	strbuf_addstr(b, "</D:propertyupdate>\n");

	if (run_request(h))
		die("set mergeinfo failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
}

static void http_start_commit(int type, const char *log, const char *path, int rev, const char *copy, int copyrev) {
	struct strbuf buf = STRBUF_INIT;
	struct request *h = &main_request;
	struct strbuf *b;

	strbuf_reset(&cmt_activity);
	strbuf_reset(&cmt_work_path);

	add_file_hdrs = curl_slist_append(add_file_hdrs, "Expect:");
	add_file_hdrs = curl_slist_append(add_file_hdrs, "Content-Type: application/vnd.svn-svndiff");

	strbuf_reset(&buf);
	strbuf_addf(&buf, "X-SVN-Version-Name: %d", rev-1);
	open_file_hdrs = curl_slist_append(open_file_hdrs, buf.buf);
	open_file_hdrs = curl_slist_append(open_file_hdrs, "Expect:");
	open_file_hdrs = curl_slist_append(open_file_hdrs, "Content-Type: application/vnd.svn-svndiff");

	if (cmt_mkactivity) {
		unsigned char actsha1[20];
		reset_request(h);
		h->method = "MKACTIVITY";

		/* need some uniqueish id for the activity */
		strbuf_addf(&cmt_activity, "%s %d %s %d %d", path, rev, copy, copyrev, (int) time(NULL));
		hash_sha1_file(cmt_activity.buf, cmt_activity.len, "blob", actsha1);
		strbuf_reset(&cmt_activity);
		strbuf_addstr(&cmt_activity, sha1_to_hex(actsha1));

		strbuf_addf(&h->url, "/!svn/act/%s", cmt_activity.buf);

		if (run_request(h))
			die("mkactivity failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);

		reset_request(h);
		strbuf_addstr(&h->url, "/!svn/vcc/default");
		strbuf_reset(&buf);
		http_checkout(h, &buf);

		reset_request(h);
		strbuf_addf(&h->url, "/!svn/ver/%d", latest_rev);
		http_checkout(h, &cmt_work_path);

		reset_request(h);
		strbuf_add(&h->url, buf.buf, buf.len);

	} else {
		static struct curl_slist *hdrs;

		if (!hdrs) {
			hdrs = curl_slist_append(hdrs, "Expect:");
			hdrs = curl_slist_append(hdrs, "Content-Type: application/vnd.svn-skel");
		}

		reset_request(h);
		h->method = "POST";
		h->hdrs = hdrs;
		h->hdrfunc = &create_commit_header;

		strbuf_addstr(&h->url, "/!svn/me");
		strbuf_addstr(&h->in.buf, "( create-txn )");

		if (run_request(h))
			die("post failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);

		strbuf_addf(&cmt_work_path, "/!svn/txr/%s", cmt_activity.buf);

		reset_request(h);
		strbuf_addf(&h->url, "/!svn/txn/%s", cmt_activity.buf);
	}

	h->method = "PROPPATCH";

	b = &h->in.buf;
	strbuf_addstr(b, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	strbuf_addstr(b, "<D:propertyupdate xmlns:D=\"DAV:\" xmlns:S=\"http://subversion.tigris.org/xmlns/svn/\">\n");
	strbuf_addstr(b, " <D:set><D:prop><S:log>");
	encode_xml(b, log);
	strbuf_addstr(b, "</S:log></D:prop></D:set>\n");
	strbuf_addstr(b, "</D:propertyupdate>\n");

	if (run_request(h))
		die("proppatch failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);

	if (type == SVN_DELETE || type == SVN_REPLACE) {
		http_delete(path);
	}

	if (type == SVN_ADD || type == SVN_REPLACE) {
		if (copyrev) {
			struct curl_slist *hdrs = NULL;

			reset_request(h);
			h->method = "COPY";

			strbuf_reset(&buf);
			strbuf_addf(&buf, "Destination: %s%s", url.buf, cmt_work_path.buf);
			append_path(&buf, path, -1);

			hdrs = curl_slist_append(hdrs, buf.buf);
			hdrs = curl_slist_append(hdrs, "Overwrite: T");
			hdrs = curl_slist_append(hdrs, "Depth: infinity");
			h->hdrs = hdrs;

			strbuf_addf(&h->url, "/!svn/bc/%d", copyrev);
			append_path(&h->url, copy, -1);

			if (run_request(h))
				die("copy failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);

			curl_slist_free_all(hdrs);
		} else {
			http_mkdir(path);
		}
	}

	strbuf_release(&buf);
}

static int http_merge_rev;
static struct strbuf *http_merge_time;

static void merge_xml_end(void *user, const char *name) {
	struct request *h = user;
	xml_end(h, name, 0);

	if (!strcmp(name, "DAV:|status")) {
		if (prefixcmp(h->cdata.buf, "HTTP/1.1 ")
		|| atoi(h->cdata.buf + strlen("HTTP/1.1")) != 200) {
			die("commit failed %s", h->cdata.buf);
		}

	} else if (http_merge_time && !strcmp(name, "DAV:|creationdate")) {
		strbuf_swap(http_merge_time, &h->cdata);

	} else if (!strcmp(name, "DAV:|version-name")) {
		http_merge_rev = atoi(h->cdata.buf);
	}

	strbuf_reset(&h->cdata);
}

static int http_finish_commit(struct strbuf *time) {
	struct request *h = &main_request;
	struct strbuf *b;

	reset_request(h);
	h->method = "MERGE";

	b = &h->in.buf;
	strbuf_addstr(b, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	strbuf_addstr(b, "<D:merge xmlns:D=\"DAV:\">\n");
	if (cmt_mkactivity) {
		strbuf_addf(b, " <D:source><D:href>%s/!svn/act/%s</D:href></D:source>\n", url.buf + pathoff, cmt_activity.buf);
	} else {
		strbuf_addf(b, " <D:source><D:href>%s/!svn/txn/%s</D:href></D:source>\n", url.buf + pathoff, cmt_activity.buf);
	}
	strbuf_addstr(b, " <D:no-auto-merge/>\n");
	strbuf_addstr(b, " <D:no-checkout/>\n");
	strbuf_addstr(b, " <D:prop>\n");
	strbuf_addstr(b, "  <D:version-name/>\n");
	strbuf_addstr(b, "  <D:creationdate/>\n");
	strbuf_addstr(b, " </D:prop>\n");
	strbuf_addstr(b, "</D:merge>\n");

	http_merge_rev = -1;
	http_merge_time = time;

	process_request(h, &xml_start, &merge_xml_end);
	if (run_request(h))
		die("merge failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);

	if (cmt_mkactivity) {
		reset_request(h);
		h->method = "DELETE";
		strbuf_addf(&h->url, "/!svn/act/%s", cmt_activity.buf);
		/* don't care whether this succeeds or not */
		run_request(h);
		latest_rev = http_merge_rev;
	}

	curl_slist_free_all(add_file_hdrs);
	curl_slist_free_all(open_file_hdrs);
	add_file_hdrs = NULL;
	open_file_hdrs = NULL;

	return http_merge_rev;
}

static void http_change_user(struct credential *cred) {
	http_auth = cred;
}

static int have_change;

static void change_xml_end(void *user, const char *name) {
	struct request *h = user;
	xml_end(h, name, 0);

	if (!strcmp(name, "svn:|log-item")) {
		have_change = 1;
	}

	strbuf_reset(&h->cdata);
}

static int http_has_change(const char *path, int start, int end) {
	struct request *h = &main_request;
	struct strbuf *b;

	reset_request(h);
	h->method = "REPORT";

	strbuf_addf(&h->url, "/!svn/ver/%d", end);
	append_path(&h->url, path, -1);

	b = &h->in.buf;
	strbuf_addstr(b, "<S:log-report xmlns:S=\"svn:\">\n");
	strbuf_addf(b, " <S:start-revision>%d</S:start-revision>\n", end);
	strbuf_addf(b, " <S:end-revision>%d</S:end-revision>\n", start);
	strbuf_addstr(b, " <S:strict-node-history/>\n");
	strbuf_addstr(b, " <S:limit>1</S:limit>\n");
	strbuf_addstr(b, " <S:no-revprops/>\n");
	strbuf_addstr(b, " <S:path/>\n");
	strbuf_addstr(b, "</S:log-report>\n");

	have_change = 0;
	process_request(h, &xml_start, &change_xml_end);

	if (run_request(h) && h->res.http_code != 404) {
		die("log failed %d %d", (int) h->res.curl_result, (int) h->res.http_code);
	}

	return have_change;
}






struct svn_proto proto_http = {
	&http_get_latest,
	&http_list,
	&http_isdir,
	&http_read_log,
	&http_read_updates,
	&http_get_mergeinfo,
	&http_start_commit,
	&http_finish_commit,
	&http_set_mergeinfo,
	&http_mkdir,
	&http_send_file,
	&http_delete,
	&http_change_user,
	&http_has_change,
	&http_cleanup,
};

struct svn_proto *svn_http_connect(struct remote *remote, struct strbuf *purl, struct credential *cred, struct strbuf *puuid, int ispush) {
	char *p = purl->buf;
	http_init(remote, p, ispush);

	http_auth = cred;
	strbuf_reset(&url);
	strbuf_addstr(&url, p);

	init_request(&main_request);

	p = strchr(url.buf + strlen(cred->protocol) + 3, '/');
	pathoff = p ? p - url.buf : url.len;

	uuid = puuid;
	http_get_options();

	p = strchr(url.buf + strlen(cred->protocol) + 3, '/');
	pathoff = p ? p - url.buf : url.len;

	strbuf_reset(purl);
	strbuf_add(purl, url.buf, url.len);

	return &proto_http;
}
