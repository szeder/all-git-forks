#ifndef GENERIC_SIGNATURE_H
#define GENERIC_SIGNATURE_H

static const char sig_header_name_generic[] = "genericsig";

extern int sign_buffer_generic(struct strbuf *buffer, struct strbuf *signature, const char *signing_key);
extern int verify_signed_buffer_generic(const char *payload, size_t payload_size, const char *signature, size_t signature_size, struct strbuf *gpg_output);
extern const char *get_signing_key_generic(void);

static const struct signature_scheme scheme_generic = 
{
  .sig_header = sig_header_name_generic,
  .sig_header_len = sizeof(sig_header_name_generic) -1,
  .sign_buffer = &sign_buffer_generic,
  .verify_signed_buffer = &verify_signed_buffer_generic,
  .get_signing_key = &get_signing_key_generic,
};


#endif
