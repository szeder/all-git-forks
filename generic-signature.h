#ifndef GENERIC_SIGNATURE_H
#define GENERIC_SIGNATURE_H

struct signature_scheme* get_scheme_generic(void);
extern int sign_buffer_generic(struct strbuf *buffer, struct strbuf *signature, const char *signing_key);
extern int verify_signed_buffer_generic(const char *payload, size_t payload_size, const char *signature, size_t signature_size, struct strbuf *gpg_output);
extern const char *get_signing_key_generic(void);

#endif
