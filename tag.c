#include "cache.h"
#include "tag.h"
#include "commit.h"
#include "tree.h"
#include "blob.h"

const char *tag_type = "tag";

struct object *deref_tag(struct object *o, const char *warn, int warnlen)
{
	while (o && o->type == OBJ_TAG)
		o = parse_object(((struct tag *)o)->tagged->sha1);
	if (!o && warn) {
		if (!warnlen)
			warnlen = strlen(warn);
		error("missing object referenced by '%.*s'", warnlen, warn);
	}
	return o;
}

struct tag *lookup_tag(const unsigned char *sha1)
{
	struct object *obj = lookup_object(sha1);
	if (!obj)
		return create_object(sha1, OBJ_TAG, alloc_tag_node());
	if (!obj->type)
		obj->type = OBJ_TAG;
        if (obj->type != OBJ_TAG) {
                error("Object %s is a %s, not a tag",
                      sha1_to_hex(sha1), typename(obj->type));
                return NULL;
        }
        return (struct tag *) obj;
}

/*
 * We refuse to tag something we can't verify. Just because.
 */
static int verify_object(unsigned char *sha1, const char *expected_type)
{
	int ret = -1;
	enum object_type type;
	unsigned long size;
	void *buffer = read_sha1_file(sha1, &type, &size);

	if (buffer) {
		if (type == type_from_string(expected_type))
			ret = check_sha1_signature(sha1, buffer, size, expected_type);
		free(buffer);
	}
	return ret;
}

/*
 * Perform parsing and verification of tag object data.
 *
 * The 'item' parameter may be set to NULL if only verification is desired.
 */
int parse_and_verify_tag_buffer(struct tag *item, const char *data, const unsigned long size, int thorough_verify)
{
#ifdef NO_C99_FORMAT
#define PD_FMT "%d"
#else
#define PD_FMT "%td"
#endif

	unsigned char sha1[20];
	char type[20];
	const char   *type_line, *tag_line, *keywords_line, *tagger_line;
	unsigned long type_len,   tag_len,   keywords_len,   tagger_len;
	const char *header_end, *end = data + size;

	if (item) {
		if (item->object.parsed)
			return 0;
		item->object.parsed = 1;
	}

	if (size < 66)
		return error("failed preliminary size check");

	/* Verify mandatory object line */
	if (memcmp(data, "object ", 7))
		return error("char%d: does not start with \"object \"", 0);

	if (get_sha1_hex(data + 7, sha1))
		return error("char%d: could not get SHA1 hash", 7);

	/* Verify mandatory type line */
	type_line = data + 48;
	if (memcmp(type_line - 1, "\ntype ", 6))
		return error("char%d: could not find \"\\ntype \"", 47);

	/* Verify optional tag line */
	tag_line = memchr(type_line, '\n', end - type_line);
	if (!tag_line++)
		return error("char" PD_FMT ": could not find \"\\n\" after \"type\"", type_line - data);
	if (end - tag_line < 4)
		return error("char" PD_FMT ": premature end of data", tag_line - data);
	if (memcmp("tag ", tag_line, 4))
		keywords_line = tag_line; /* no tag name given */
	else {                            /* tag name given */
		keywords_line = memchr(tag_line, '\n', end - tag_line);
		if (!keywords_line++)
			return error("char" PD_FMT ": could not find \"\\n\" after \"tag\"", tag_line - data);
	}

	/* Verify optional keywords line */
	if (end - keywords_line < 9)
		return error("char" PD_FMT ": premature end of data", keywords_line - data);
	if (memcmp("keywords ", keywords_line, 9))
		tagger_line = keywords_line; /* no keywords given */
	else {                               /* keywords given */
		tagger_line = memchr(keywords_line, '\n', end - keywords_line);
		if (!tagger_line++)
			return error("char" PD_FMT ": could not find \"\\n\" after \"keywords\"", keywords_line - data);
	}

	if (thorough_verify) {
		/*
		 * Verify mandatory tagger line, but only when we're checking
		 * thoroughly, i.e. on inserting a new tag, and on fsck.
		 * There are existing tag objects without a tagger line (most
		 * notably the "v0.99" tag in the main git repo), and we don't
		 * want to fail parsing on these.
		 */
		if (end - tagger_line < 7)
			return error("char" PD_FMT ": premature end of data", tagger_line - data);
		if (memcmp("tagger ", tagger_line, 7))
			return error("char" PD_FMT ": could not find \"tagger \"", tagger_line - data);
		header_end = memchr(tagger_line, '\n', end - tagger_line);
		if (!header_end++)
			return error("char" PD_FMT ": could not find \"\\n\" after \"tagger\"", tagger_line - data);
		if (end - header_end < 1)
			return error("char" PD_FMT ": premature end of data", header_end - data);
		if (*header_end != '\n') /* header must end with "\n\n" */
			return error("char" PD_FMT ": could not find blank line after header section", header_end - data);
	}
	else {
		/* Treat tagger line as optional */
		if (end - tagger_line >= 7 && !memcmp("tagger ", tagger_line, 7)) {
			/* Found tagger line */
			header_end = memchr(tagger_line, '\n', end - tagger_line);
			if (!header_end++)
				return error("char" PD_FMT ": could not find \"\\n\" after \"tagger\"", tagger_line - data);
		}
		else /* No tagger line */
			header_end = tagger_line;
	}

	if (end - header_end < 1)
		return error("char" PD_FMT ": premature end of data", header_end - data);
	if (*header_end != '\n') /* header must end with "\n\n" */
		return error("char" PD_FMT ": could not find blank line after header section", header_end - data);

	/* Calculate lengths of header fields */
	type_len      = tag_line      == type_line ? 0 :     /* 0 if not given, > 0 if given */
			(tag_line      - type_line)     - strlen("type \n");
	tag_len       = keywords_line == tag_line ? 0 :      /* 0 if not given, > 0 if given */
			(keywords_line - tag_line)      - strlen("tag \n");
	keywords_len  = tagger_line   == keywords_line ? 0 : /* 0 if not given, > 0 if given */
			(tagger_line   - keywords_line) - strlen("keywords \n");
	tagger_len    = header_end    == tagger_line ? 0 :   /* 0 if not given, > 0 if given */
			(header_end    - tagger_line)   - strlen("tagger \n");

	/* Get the actual type */
	if (type_len >= sizeof(type))
		return error("char" PD_FMT ": type too long", (type_line + 5) - data);
	memcpy(type, type_line + 5, type_len);
	type[type_len] = '\0';

	if (thorough_verify) {
		/* Verify that the object matches */
		if (verify_object(sha1, type))
			return error("char%d: could not verify object %s", 7, sha1_to_hex(sha1));

		/* Verify the tag name: we don't allow control characters or spaces in it */
		if (tag_len > 0) { /* tag name was given */
			tag_line += 4; /* skip past "tag " */
			for (;;) {
				unsigned char c = *tag_line++;
				if (c == '\n')
					break;
				if (c > ' ' && c != 0x7f)
					continue;
				return error("char" PD_FMT ": could not verify tag name", tag_line - data);
			}
		}

		/* Verify the keywords line: we don't allow control characters or spaces in it, or two subsequent commas */
		if (keywords_len > 0) { /* keywords line was given */
			keywords_line += 9; /* skip past "keywords " */
			for (;;) {
				unsigned char c = *keywords_line++;
				if (c == '\n')
					break;
				if (c == ',' && *keywords_line == ',')
					return error("char" PD_FMT ": found empty keyword", keywords_line - data);
				if (c > ' ' && c != 0x7f)
					continue;
				return error("char" PD_FMT ": could not verify keywords", keywords_line - data);
			}
		}

		/* Verify the tagger line */
		/* TODO: check for committer/tagger info */

		/* The actual stuff afterwards we don't care about.. */
	}

	if (item) { /* Store parsed information into item */
		if (tag_len > 0) { /* optional tag name was given */
			item->tag = xmalloc(tag_len + 1);
			memcpy(item->tag, tag_line + 4, tag_len);
			item->tag[tag_len] = '\0';
		}
		else { /* optional tag name not given */
			item->tag = xmalloc(1);
			item->tag[0] = '\0';
		}

		if (keywords_len > 0) { /* optional keywords string was given */
			item->keywords = xmalloc(keywords_len + 1);
			memcpy(item->keywords, keywords_line + 9, keywords_len);
			item->keywords[keywords_len] = '\0';
		}
		else { /* optional keywords string not given. Set default keywords */
			/* if tag name is set, use "tag"; otherwise use "note" */
			const char *default_kw = item->tag ? "tag" : "note";
			item->keywords = xmalloc(strlen(default_kw) + 1);
			memcpy(item->keywords, default_kw, strlen(default_kw) + 1);
		}

		if (!strcmp(type, blob_type)) {
			item->tagged = &lookup_blob(sha1)->object;
		} else if (!strcmp(type, tree_type)) {
			item->tagged = &lookup_tree(sha1)->object;
		} else if (!strcmp(type, commit_type)) {
			item->tagged = &lookup_commit(sha1)->object;
		} else if (!strcmp(type, tag_type)) {
			item->tagged = &lookup_tag(sha1)->object;
		} else {
			error("Unknown type %s", type);
			item->tagged = NULL;
		}

		if (item->tagged && track_object_refs) {
			struct object_refs *refs = alloc_object_refs(1);
			refs->ref[0] = item->tagged;
			set_object_refs(&item->object, refs);
		}
	}
	return 0;

#undef PD_FMT
}

int parse_tag_buffer(struct tag *item, void *data, unsigned long size)
{
	return parse_and_verify_tag_buffer(item, (const char *) data, size, 0);
}

int parse_tag(struct tag *item)
{
	enum object_type type;
	void *data;
	unsigned long size;
	int ret;

	if (item->object.parsed)
		return 0;
	data = read_sha1_file(item->object.sha1, &type, &size);
	if (!data)
		return error("Could not read %s",
			     sha1_to_hex(item->object.sha1));
	if (type != OBJ_TAG) {
		free(data);
		return error("Object %s not a tag",
			     sha1_to_hex(item->object.sha1));
	}
	ret = parse_tag_buffer(item, data, size);
	free(data);
	return ret;
}
