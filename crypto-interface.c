#include "cache.h"
#include "run-command.h"
#include "strbuf.h"
#include "crypto-interface.h"
#include "sigchain.h"
#include <stdlib.h>

void crypto_set_signing_key(const char *key)
{
    system ("echo \"crypto-set-signing-key\"");
}

int crypto_git_config(const char *var, const char *value, void *cb)
{
    system ("echo \"crypto-git-config\"");
	return 0;
}

const char *crypto_get_signing_key(void)
{
    system ("echo \"crypto-get-signing_key\"");
	return "Some dummy key";
}

/*
 * Create a detached signature for the contents of "buffer" and append
 * it after "signature"; "buffer" and "signature" can be the same
 * strbuf instance, which would cause the detached signature appended
 * at the end.
 */
int crypto_sign_buffer(struct strbuf *buffer, struct strbuf *signature, const char *signing_key)
{
    system ("echo \"crypto-sign_buffer\"");
	return 0;
}

/*
 * Run  to see if the payload matches the detached signature.
 */
int crypto_verify_signed_buffer(const char *payload, size_t payload_size,
        const char *signature, size_t signature_size,
        struct strbuf *gpg_output){
    system ("echo \"crypto-verify_signed_buffer\"");
	return 0;
}
