/*
primitive caching mechanism for git-daemon

acts as a middle-man between upload-pack and pack-objects.  it profiles 
each request to a cache file, and attempts to make use of it if found.
*/
#include "cache.h"
#include "refs.h"
#include "tag.h"
#include "object.h"
#include "commit.h"
#include "exec_cmd.h"
#include "diff.h"
#include "revision.h"
#include "list-objects.h"
#include "run-command.h"
#include "strbuf.h"
#include "reflog-walk.h"

struct daemon_cache {
	unsigned long timestamp;
	unsigned long filesize;
	unsigned char head_sha1[20];
	
	int nheads;
	struct strbuf heads;
};

enum cache_status {
	DAEMON_CACHE_FRESH, /* directly stream */
	DAEMON_CACHE_STALE, /* pack differences */
	DAEMON_CACHE_ROTTEN /* rewrite the whole thing */
};

char create_full_pack = 0;
struct daemon_cache *metadata = 0;
struct strbuf want_list = { 0 }, have_list = { 0 };
const char *g_argv[20];
unsigned long g_lasttimestamp;

static int add_ref_to_hash(const char *ref, const unsigned char *sha1, int flags, void *extra)
{
	git_SHA1_Update((git_SHA_CTX *)extra, sha1, 20);
	
	return 0;
}

static char *get_head_sha1(void)
{
	static unsigned char sha1[20], dunnit = 0;
	git_SHA_CTX c;
	
	if (dunnit) 
		return sha1;
	else 
		dunnit = 1;
	
	git_SHA1_Init(&c);
	/* should we be ordering these explicitly? */
	for_each_branch_ref(add_ref_to_hash, (void *)&c);
	git_SHA1_Final(sha1, &c);
	
	return sha1;
}

static char *get_cache_name(void)
{
	/* analyze on the basis wants/haves?
	 * eg. cache for commits between tags? (between releases)
	 */
	
	if (create_full_pack)
		return "clone";
	
	return 0;
}

/* reads output from pack-objects and passes it on to upload-pack
 * also writes data to cache_fd if it's >= 0
 */
static int middle_man_stream(int cache_fd)
{
	struct child_process pack_objects;
	char data[8193];
	int retval = 0;
	
	memset(&pack_objects, 0, sizeof(pack_objects));
	pack_objects.out = cache_fd < 0 ? 0 : -1;
	pack_objects.err = 0;
	pack_objects.in = -1;
	pack_objects.git_cmd = 1;
	pack_objects.argv = g_argv;

	if (start_command(&pack_objects))
		die("git upload-pack: unable to fork git-pack-objects");
	
	if (want_list.len)
		xwrite(pack_objects.in, want_list.buf, want_list.len);
	if (have_list.len) {
		xwrite(pack_objects.in, "--not\n", 6);
		xwrite(pack_objects.in, have_list.buf, have_list.len);
	}
	xwrite(pack_objects.in, "\n", 1);
	
	if (cache_fd < 0) 
		goto skip_middle_man;
	
	while (1) {
		int sz;
		sz = read(pack_objects.out, data, sizeof(data));
		if (sz == 0) {
			close(pack_objects.out);
			break;
		}
		else if (sz < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				sleep(1);
				continue;
			}
			else 
				goto fail;
		}
		xwrite(1, data, sz);
		write(cache_fd, data, sz);
		retval += sz;
	}

skip_middle_man:
	if (finish_command(&pack_objects)) {
		error("git daemon-cache: git-pack-objects died with error.");
		goto fail;
	}
	
	return retval;
	
fail:
	/* just bail out; it's more important that we don't sneak corrupted data to our client */
	die("OMG WHAT HAPPENED TO MAH PIPE!?");
}

/* read to stdout */
static int file_stream(int cache_fd)
{
	char data[8193];
	int retval = 0;
	
	if (cache_fd < 0)
		return 0;
	
	while (1) {
		int sz;
		sz = read(cache_fd, data, sizeof(data));
		if (sz > 0)
			xwrite(1, data, sz);
		else if (sz == 0)
			break;
		else
			goto fail;
		retval += sz;
	}
	
	return retval;
	
fail:
	die("OMG WHAT HAPPENED TO MAH FILE!?");
}

static int add_ref_to_metadata(const char *ref, const unsigned char *sha1, int flags, void *extra)
{
	struct daemon_cache *dc = (struct daemon_cache *)extra;
	
	strbuf_addf(&dc->heads, "%s\n", sha1_to_hex(sha1));
	dc->nheads++;
	
	return 0;
}

/* todo: use temp file? */
static int write_cache_metadata(char *name, struct daemon_cache *dc)
{
	FILE *fd;
	
	fd = fopen(git_path("daemon-cache/%s-meta", name), "w");
	if (!fd)
		goto bad;
	
	fprintf(fd, "%lu %lu %d\n%s\n", dc->timestamp, dc->filesize, dc->nheads, sha1_to_hex(dc->head_sha1));
	fprintf(fd, "%s", dc->heads.buf);
	
	fclose(fd);
	return 0;
	
bad:
	return -1;
}

static int read_cache_metadata(char *name, struct daemon_cache **dcp)
{
	FILE *fd;
	char line[1024], sha1[20];
	struct daemon_cache *dc;
	int i;
	
	*dcp = dc = xcalloc(sizeof(struct daemon_cache), 1);
	fd = fopen(git_path("daemon-cache/%s-meta", name), "r");
	if (!fd)
		goto verybad;
	
	/* general data */
	if (!fgets(line, sizeof(line), fd))
		goto bad;
	sscanf(line, "%lu %lu %d", &dc->timestamp, &dc->filesize, &dc->nheads);
	if (!dc->nheads || !dc->filesize)
		goto bad;
	if (!fgets(line, sizeof(line), fd) || get_sha1_hex(line, dc->head_sha1))
		goto bad;
	
	/* read in heads */
	for (i = 0; i < dc->nheads; i++) {
		if (!fgets(line, sizeof(line), fd) || get_sha1_hex(line, sha1))
			break;
		else
			strbuf_add(&dc->heads, line, 41);
	}
	
	if (i != dc->nheads) 
		goto bad;
	fclose(fd);
	
	return 0;
	
bad:
	fclose(fd);
 verybad:
	
	return -1;
}

static int check_reflog(unsigned char *osha1, unsigned char *nsha1, 
	const char *email, unsigned long timestamp, 
	int tz, const char *message, void *extra)
{
	static int gotthere = 0;
	int *info = (int *)extra;
	
	g_lasttimestamp = timestamp;
	if (!gotthere) {
		if (!metadata->timestamp || timestamp == metadata->timestamp) {
			gotthere = 1;
			*info |= 0x02;
		}
		return 0;
	}
	
	if (!strncmp(message, "rebase:", 7) 
		|| !strncmp(message, "commit (amend):", 15)) {
		*info |= 0x01;
	}
	
	return 0;
}

/* returns 0 on success and -1 on failure */
static enum cache_status get_cache_status(char *name, int *fd)
{
	struct stat fi;
	enum cache_status retval = 0;
	
	/* rudimentary checks */
	*fd = -1;
	if (read_cache_metadata(name, &metadata)
		|| (*fd = open(git_path("daemon-cache/%s", name), O_RDONLY)) < 0
		|| fstat(*fd, &fi) 
		|| fi.st_size != metadata->filesize
	) {
		retval = DAEMON_CACHE_ROTTEN;
		if (*fd >= 0) 
			close(*fd);
	}
	else if (!memcmp(metadata->head_sha1, get_head_sha1(), 20))
		return DAEMON_CACHE_FRESH;
	
	/* later on we might do something more sophisticated with history rewrites... */
	int extra = 0;
	g_lasttimestamp = 0;
	for_each_reflog_ent("HEAD", check_reflog, &extra);
	
	if (!(extra & 0x02) || extra & 0x01)
		retval = DAEMON_CACHE_ROTTEN;
	
	if (!retval) 
		retval = DAEMON_CACHE_STALE;
	else if (retval == DAEMON_CACHE_ROTTEN) {
		strbuf_release(&metadata->heads);
		memset(metadata, 0, sizeof(struct daemon_cache));
		metadata->timestamp = g_lasttimestamp;
	}
	
	return retval;
}

static int daemon_cache(void)
{
	int retval, fd;
	char tmpfile[PATH_MAX];
	char *name;
	
	name = get_cache_name();
	
	if (!name)
		return middle_man_stream(-1);
	
	switch(get_cache_status(name, &fd))
	{
		case DAEMON_CACHE_FRESH : 
			fprintf(stderr, "streaming from cache\n");
			retval = file_stream(fd);
			close(fd);
			break;
			
		case DAEMON_CACHE_STALE : 
			fprintf(stderr, "supplimenting cache\n");
			retval = file_stream(fd);
			close(fd);
			
			strbuf_add(&have_list, metadata->heads.buf, metadata->heads.len);
			retval += middle_man_stream(-1);
			break;
			
		case DAEMON_CACHE_ROTTEN : 
			fprintf(stderr, "repacking\n");
			fd = open(git_path("daemon-cache/%s", name), O_CREAT | O_WRONLY | O_TRUNC, 0666);
			/* strcpy(tmpfile, git_path("daemon-cache/%s_XXXXXX", name));
			fd = xmkstemp(tmpfile); */
			
			retval = middle_man_stream(fd);
			if (fd >= 0) {
				close(fd);
				/* move_temp_to_file(tmpfile, git_path("daemon-cache/%s", name));
				chmod(git_path("daemon-cache/%s", name), 0666); */
			}
			
			for_each_branch_ref(add_ref_to_metadata, (void *)metadata);
			memcpy(metadata->head_sha1, get_head_sha1(), 20);
			metadata->filesize = retval;
			write_cache_metadata(name, metadata);
			break;
	}
	
	strbuf_release(&metadata->heads);
	free(metadata);
	
	return retval;
}

static void fill_lists(void)
{
	struct strbuf *curlist = &want_list;
	char line[1024];
	
	while (fgets(line, sizeof(line), stdin)) {
		if (*line == '\n')
			break;
		if (*line == '-' && !strcmp(line, "--not")) {
			curlist = &have_list;
			continue;
		}
		
		strbuf_add(curlist, line, strlen(line));
	}
	
}

int main(int argc, char *argv[])
{
	int i;
	
	g_argv[0] = "pack-objects";
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--all"))
			create_full_pack = 1;
		g_argv[i] = argv[i];
	}
	g_argv[i] = 0;
	
	fill_lists();
	daemon_cache();
	
	strbuf_release(&want_list);
	strbuf_release(&have_list);
	
	return 0;
}
