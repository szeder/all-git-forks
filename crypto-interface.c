#include "cache.h"
#include "run-command.h"
#include "strbuf.h"
#include "crypto-interface.h"
#include "sigchain.h"
#include <stdlib.h>

#define BASH_ERROR -1

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
int crypto_sign_buffer(char * pem)
{
    char * script = "HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1); \
                     FILE=$(date +\%s).txt; \
                     git show $(HASH) > \"$FILE\"; \
                     openssl cms -sign -in \"$FILE\" -text -out \"$FILE\" -signer myCert.pem ; \
                     git notes --ref=crypto add -F \"$FILE\" HEAD; \
                     rm \"$FILE\"; \
                     echo \"Pushing signed note to the origin\"; \
                     git push origin refs/notes/crypto/*; ";
    int bashResult = system(script);
    if(bashResult == BASH_ERROR)
        printf("Error fetching signature, signing, adding to notes/n");
	return 0;
}

/*
 * Run  to see if the payload matches the detached signature.
 */
int crypto_verify_signed_buffer( char * pem ){
    system ("echo \"crypto-verify_signed_buffer\"");
	return 0;
}
