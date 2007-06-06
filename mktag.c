#include "cache.h"
#include "tag.h"

/*
 * Tag object data has the following format: two mandatory lines of
 * "object <sha1>" + "type <typename>", plus two optional lines of
 * "tag <tagname>" + "keywords <keywords>", plus a mandatory line of
 * "tagger <committer>", followed by a blank line, a free-form tag
 * message and an optional signature block that git itself doesn't
 * care about, but that can be verified with gpg or similar.
 *
 * <sha1> represents the object pointed to by this tag, <typename> is
 * the type of the object pointed to ("tag", "blob", "tree" or "commit"),
 * <tagname> is the name of this tag object (and must correspond to the
 * name of the corresponding ref (if any) in '.git/refs/'). <keywords> is
 * a comma-separated list of keywords associated with this tag object, and
 * <committer> holds the "name <email>" of the tag creator and timestamp
 * of when the tag object was created (analogous to "committer" in commit
 * objects).
 *
 * The first two lines are guaranteed to be at least 57 bytes:
 * "object <sha1>\n" is 48 bytes, and "type tag\n" at 9 bytes is
 * the shortest possible "type" line. The tagger line is at least
 * "tagger \n" (8 bytes), and a blank line is also needed (1 byte).
 * Therefore a tag object _must_ have >= 66 bytes.
 *
 * If "tag <tagname>" is omitted, <tagname> defaults to the empty string.
 * If "keywords <keywords>" is omitted, <keywords> defaults to "tag" if
 * a <tagname> was given, "note" otherwise.
 */

int main(int argc, char **argv)
{
	unsigned long size = 4096;
	char *buffer = xmalloc(size);
	struct tag result_tag;
	unsigned char result_sha1[20];

	if (argc != 1)
		usage("git-mktag < tag_data_file");

	setup_git_directory();

	if (read_pipe(0, &buffer, &size)) {
		free(buffer);
		die("could not read from stdin");
	}

	/* Verify tag object data */
	if (parse_and_verify_tag_buffer(&result_tag, buffer, size, 1)) {
		free(buffer);
		die("invalid tag data file");
	}

	if (write_sha1_file(buffer, size, tag_type, result_sha1) < 0) {
		free(buffer);
		die("unable to write tag file");
	}

	free(buffer);
	printf("%s\n", sha1_to_hex(result_sha1));
	return 0;
}
