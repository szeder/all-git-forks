#include "cache.h"
#include "run-command.h"
#include "strbuf.h"
#include "crypto-interface.h"
#include "sigchain.h"
#include <stdlib.h>
#include "notes.h"
#include "string-list.h"

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
int crypto_sign_buffer( )
{
    char * script = "HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1); \
                     FILE=$(date +\%s); \
                     git show $(HASH) > \"$FILE\".txt; \
                     openssl cms -sign -in \"$FILE\".txt -text -out \"$FILE\".msg -signer ~/myCert.pem ; \
                     git notes --ref=crypto add -F \"$FILE\".msg HEAD; \
                     rm \"$FILE\".txt \"$FILE\".msg; ";
    char * extra =  "echo \"Pushing signed note to the origin\"; \
                     git push origin refs/notes/crypto/*; ";
    int bashResult = system(script);
    if(bashResult == BASH_ERROR)
        printf("Error fetching signature, signing, adding to notes\n");
	return 0;
}

/*
 * Run  to see if the payload matches the detached signature.
 */
int crypto_verify_signed_buffer(  )
{
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

// Helper function which does "--ref=crypto"
void set_notes_ref(const char * ref)
{
    struct strbuf sb = STRBUF_INIT;
    strbuf_addstr(&sb, ref);
    expand_notes_ref(&sb);
    setenv("GIT_NOTES_REF", sb.buf, 1);
}

const unsigned char * get_note_for_commit(const char * commit_ref)
{
    struct notes_tree *t;
    unsigned char object[20];
    const unsigned char * note;

    // convert the hex to the commit object
    if(get_sha1(commit_ref, object))
        die(_("Failed to resolve '%s' as a valid ref."), commit_ref);

    // Set the ENV to the right namespace
    set_notes_ref("crypto");

    // Since the env is set &default_notes_tree points at crypto
    init_notes(NULL, NULL, NULL, 0);
    t = &default_notes_tree;

    // Get our note
    note = get_note(t, object);
    if(!note)
        return 0;
    return sha1_to_hex(note); // return the sha ref
}

char * get_note_from_sha1(const char * ref)
{
    unsigned char sha1[20];
    enum object_type type;
    unsigned long size;
    char * buf;
    //void * data; // Unused for now

    if(get_sha1(ref, sha1)){
        die(_("Failed to resolve '%s' as a valid ref."), ref);
    }

    // Get the type and size of the object(note)
    type = sha1_object_info(sha1, &size);
    // Get the pretty char *
    buf = read_sha1_file(sha1, &type, &size);

    // This line should return the blob but it's a function we
    // dont have access too easily
    //data = read_object(sha1, type, size);

    if(!buf)
        die("Cannot read object %s", ref);
    return buf;
}

	//return stream_blob_to_fd(1, sha1, NULL, 0);
