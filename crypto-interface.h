#ifndef CYPTO_INTERFACE_H
#define CYPTO_INTERFACE_H

extern int cypto_sign_buffer(struct strbuf *buffer, struct strbuf *signature, const char *signing_key);
extern int cypto_verify_signed_buffer(const char *payload, size_t payload_size, const char *signature, size_t signature_size, struct strbuf *gpg_output);
extern int cypto_git_config(const char *, const char *, void *);
extern void cypto_set_signing_key(const char *);
extern const char * cypto_get_signing_key(void);

#endif
