#ifndef SIGNATURE_INTERFACE_H
#define SIGNATURE_INTERFACE_H

struct signature_scheme {
  const char* sig_header;
  int sig_header_len;
  int (*sign_buffer)(struct strbuf*, struct strbuf*, const char*);
  int (*verify_signed_buffer)(const char *, size_t, const char*, size_t, struct strbuf*);
  const char* (*get_signing_key)(void);
};

const char* parse_for_signature(char* line);
const char* get_sig_header();
int get_sig_header_len();
extern int sign_buffer(struct strbuf *buffer, struct strbuf *signature, const char *signing_key);
extern int verify_signed_buffer(const char *payload, size_t payload_size, const char *signature, size_t signature_size, struct strbuf *gpg_output);
extern const char *get_signing_key(void);

#endif
