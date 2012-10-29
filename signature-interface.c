#include "cache.h"
#include "signature-interface.h"
#include "gpg-interface.h"
#include "generic-signature.h"

#define signature_mode "commandline"

struct signature_scheme* get_scheme()
{
  if(strcmp(signature_mode, "genericsig")){
    return get_scheme_generic();
  }else{
    //default signature scheme is gpg
    return get_scheme_gpg();
  }
}

struct signature_scheme** get_schemes()
{
  struct signature_scheme *schemes[2] = {
    get_scheme_generic(), 
    get_scheme_gpg()
  };
  return schemes;
}

int get_schemes_count()
{
  return 2;
}

const char* get_sig_header()
{
  return get_scheme()->sig_header;
}

int get_sig_header_len()
{
  return get_scheme()->sig_header_len;
}

const char* parse_for_signature(char* line)
{
  struct signature_scheme* schemes = get_schemes();
  int i;
  for(i=0;i<get_schemes_count();++i) {
    struct signature_scheme scheme = schemes[i];
    if (!prefixcmp(line, scheme.sig_header) &&
			 line[scheme.sig_header_len] == ' ')
      return (line + scheme.sig_header_len + 1);
  }
  return NULL;
}

/*
 * Create a detached signature for the contents of "buffer" and append
 * it after "signature"; "buffer" and "signature" can be the same
 * strbuf instance, which would cause the detached signature appended
 * at the end.
 */
int sign_buffer(struct strbuf *buffer, struct strbuf *signature, const char *signing_key)
{
  return (get_scheme()->sign_buffer)(buffer, signature, signing_key);
}

/*
 * Run "gpg" to see if the payload matches the detached signature.
 * gpg_output, when set, receives the diagnostic output from GPG.
 */
int verify_signed_buffer(const char *payload, size_t payload_size,
			 const char *signature, size_t signature_size,
			 struct strbuf *gpg_output)
{
  return (get_scheme()->verify_signed_buffer)(payload, payload_size, 
					      signature, signature_size,
					      gpg_output);
}

const char *get_signing_key(void)
{
  return (get_scheme()->get_signing_key)();
}


