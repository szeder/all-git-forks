#ifndef GPG_INTERFACE_H
#define GPG_INTERFACE_H

/*
 * Copyright (c) 2011, Google Inc.
 */

extern int sign_buffer(struct strbuf *buffer, const char *signing_key);
extern int verify_signed_buffer(const char *buffer, size_t total, size_t payload);
extern int git_gpg_config(const char *, const char *, void *);
extern void set_signing_key(const char *);
extern const char *get_signing_key(void);

#endif
