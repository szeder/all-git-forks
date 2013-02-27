#ifndef CYPTO_INTERFACE_H
#define CYPTO_INTERFACE_H

extern int crypto_sign_buffer();
extern int crypto_verify_signed_buffer();
extern int crypto_git_config(const char *, const char *, void *);
extern void crypto_set_signing_key(const char *);
extern const char * crypto_get_signing_key(void);

extern char ** get_commit_list();

/**
 * Given a reference to a commit this function looks for an
 * associated note in the crypto notes namespace
 *
 * If one is found the sha1 ref is returned
 * If none is found 0 is returned
 **/
extern const unsigned char * get_note_for_commit(const char *);

/**
 * Given the sha1 of a note this function returns the
 *  pretty char* of the note.
 *
 * If no note is found this dies.
 */
char * get_note_from_sha1(const char *);
#endif
