#include "cache.h"
#include "gpg-interface.h"
#include "generic-signature.h"

#define signature_mode "commandline"
/*
 * Create a detached signature for the contents of "buffer" and append
 * it after "signature"; "buffer" and "signature" can be the same
 * strbuf instance, which would cause the detached signature appended
 * at the end.
 */
int sign_buffer(struct strbuf *buffer, struct strbuf *signature, const char *signing_key)
{
  if(strcmp(signature_mode, "commandline")){
    return sign_buffer_commandline(buffer, signature, signing_key);
  }else{
    //default signature scheme is gpg
    return sign_buffer_gpg(buffer, signature, signing_key);
  }
}

/*
 * Run "gpg" to see if the payload matches the detached signature.
 * gpg_output, when set, receives the diagnostic output from GPG.
 */
int verify_signed_buffer(const char *payload, size_t payload_size,
			 const char *signature, size_t signature_size,
			 struct strbuf *gpg_output)
{
  if(strcmp(signature_mode, "commandline")){
    return verify_signed_buffer_commandline(payload, payload_size, 
					    signature, signature_size,
					    gpg_output);
  }else{
    //default signature scheme is gpg
    return verify_signed_buffer_gpg(payload, payload_size, 
					    signature, signature_size,
					    gpg_output);

  }

const char *get_signing_key(void)
  {
  if(strcmp(signature_mode, "commandline")){
    return get_signing_key_commandline();
  }else{
    //default signature scheme is gpg
    return get_signing_key_gpg();
  }

  }
}
