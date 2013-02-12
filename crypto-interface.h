#ifndef CYPTO_INTERFACE_H
#define CYPTO_INTERFACE_H

extern int crypto_sign_buffer();
extern int crypto_verify_signed_buffer();
extern int crypto_git_config(const char *, const char *, void *);
extern void crypto_set_signing_key(const char *);
extern const char * crypto_get_signing_key(void);

#endif
