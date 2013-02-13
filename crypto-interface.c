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
                     FILE=$(date +\%s); \
                     git show $(HASH) > \"$FILE\".txt; \
                     openssl cms -sign -in \"$FILE\".txt -text -out \"$FILE\".msg -signer myCert.pem ; \
                     git notes --ref=crypto add -F \"$FILE\".msg HEAD; \
                     rm \"$FILE\".txt \"$FILE\".msg; \
                     echo \"Pushing signed note to the origin\"; \
                     git push origin refs/notes/crypto/*; ";
    int bashResult = system(script);
    if(bashResult == BASH_ERROR)
        printf("Error fetching signature, signing, adding to notes\n");
	return 0;
}

/*
 * Run  to see if the payload matches the detached signature.
 */
int crypto_verify_signed_buffer( char * pem ){
    char * script = "NOTEID=$(git notes --ref=crypto | cut -d ' ' -f 1 | head -n1); \
                    COMMITID=$(git notes --ref=crypto | cut -d ' ' -f 2 | head -n1); \
                    TIME=$(date +\%s); \
                    git show $NOTEID > \"$TIME\".msg; \
                    git show $COMMITID > \"$TIME\".cmt; \
                    openssl smime -verify -in \"$TIME\".msg -noverify -signer \"$TIME\".pem -out \"$TIME\".data 2> /dev/null; \
                    echo \"Verifying the signature: ...\"; \
                    openssl cms -verify -in \"$TIME\".msg -noverify -signer \"$TIME\".pem -out \"$TIME\".data; \
                    tail -n +3 \"$TIME\".data > \"$TIME\"2.data; \
                    if diff -w --brief <(sort \"$TIME\".cmt) <(sort \"$TIME\"2.data) > /dev/null ; then \
                        echo \"Verification passed for commit: $COMMITID\"; \
                    else \
                        echo \"Verification FAILS for commit: $COMMITID\"; \
                    fi; \
                    rm \"$TIME\"*;";
    int bashResult = system("./verifyLiteCMS.sh");
    //if(bashResult == BASH_ERROR);
    //    printf("Error fetching signature for the head commit and verifying\n");
	return 0;
}
