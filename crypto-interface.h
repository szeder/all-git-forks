#ifndef CYPTO_INTERFACE_H
#define CYPTO_INTERFACE_H

#define VERIFY_PASS             0
#define VERIFY_FAIL_NO_NOTE     1
#define VERIFY_FAIL_BAD_SIG     2
#define VERIFY_FAIL_NOT_TRUSTED 4
#define VERIFY_FAIL_COMPARE     8

extern int crypto_sign_buffer();
extern int crypto_git_config(const char *, const char *, void *);
extern void crypto_set_signing_key(const char *);
extern const char * crypto_get_signing_key(void);
extern int calc_sha256 (char* path, char output[65]);
extern void sha256_hash_string (unsigned char hash[SHA256_DIGEST_LENGTH], char outputBuffer[65]);

/**
 * get_commit_list returns an array of char*
 * the last item in the array is NULL
 **/
extern char ** get_commit_list();


/**
 * verify_commit
 * given a sha1 ref to a commit this verifies the note
 * if there is one.
 **/
extern int verify_commit(char * sha1);


#endif
