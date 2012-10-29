#include "cache.h"
#include "strbuf.h"
#include "signature-interface.h"
#include "generic-signature.h"

struct signature_scheme* get_scheme_generic(void)
{
  static struct signature_scheme scheme;
  if(scheme.sign_buffer == NULL){
    //initialize the structure
    scheme.sig_header = "genericsig";
    scheme.sig_header_len = strlen(scheme.sig_header);
    scheme.sign_buffer = &sign_buffer_generic;
    scheme.verify_signed_buffer = &verify_signed_buffer_generic;
    scheme.get_signing_key = &get_signing_key_generic;
  }

  return &scheme;
}

/*
 * Create a detached signature for the contents of "buffer" and append
 * it after "signature"; "buffer" and "signature" can be the same
 * strbuf instance, which would cause the detached signature appended
 * at the end.
 */
int sign_buffer_generic(struct strbuf *buffer, struct strbuf *signature, const char *signing_key)
{
  char* signature_string = "My signature\n";
  signature->len = strlen(signature_string);
  signature->buf = signature_string;
  return 0;
}

/*
 * Run "gpg" to see if the payload matches the detached signature.
 * gpg_output, when set, receives the diagnostic output from GPG.
 */
int verify_signed_buffer_generic(const char *payload, size_t payload_size,
			 const char *signature, size_t signature_size,
			 struct strbuf *gpg_output)
{
if(strcmp(signature,"My signature\n"))
    return 0;
  else
    return 1;
}

const char *get_signing_key_generic(void)
{
return "Foo's key.";
}
