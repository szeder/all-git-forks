#ifndef MAILINFO_H
#define MAILINFO_H

struct mailinfo_opts {
	int keep_subject;
	int keep_non_patch_brackets_in_subject;
	const char *metainfo_charset;
	int use_scissors;
	int use_inbody_headers;
	int add_message_id;
};

extern void mailinfo_opts_init(struct mailinfo_opts *opts);

extern int mailinfo(const struct mailinfo_opts *opts, FILE *in, FILE *out,
		const char *msg, const char *patch);

#endif
