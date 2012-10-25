#ifndef COMMANDLINE_SIGNATURE_H
#define COMMANDLINE_SIGNATURE_H

extern int sign_buffer(struct strbuf *buffer, struct strbuf *signature, const char *signing_key);
extern int verify_signed_buffer(const char *payload, size_t payload_size, const char *signature, size_t signature_size, struct strbuf *gpg_output);

#endif
