#ifndef SIGNATURE_INTERFACE_H
#define SIGNATURE_INTERFACE_H

typedef int (*sign_buffer_function)(struct strbuf*, struct strbuf*, const char*);
typedef int (*verify_signed_buffer_function)(const char *, size_t, const char*, size_t, struct strbuf*);
typedef  const char* (*get_signing_key_function)(void);


struct signature_scheme {
  const char* sig_header;
  const int sig_header_len;
  const sign_buffer_function sign_buffer;
  const verify_signed_buffer_function verify_signed_buffer;
  const get_signing_key_function get_signing_key;
};

const char* parse_for_signature(char* line);
const char* get_sig_header();
int get_sig_header_len();
extern int sign_buffer(struct strbuf *buffer, struct strbuf *signature, const char *signing_key);
extern int verify_signed_buffer(const char *payload, size_t payload_size, const char *signature, size_t signature_size, struct strbuf *gpg_output);
extern const char *get_signing_key(void);

#endif
