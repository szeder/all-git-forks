//
//  sign.c
//
//
//  Created by Vincent Clasgens on 1/30/13.
//  Modified by Dustin Dalen on 2/4/13.
//  Modified by Vincent Clasgens 3/26/13
//  University of Portland (Team Rogue)
//
//
#include <stdio.h>
#include "builtin.h"
#include "builtin/config.h"
#include "commit.h"
#include "crypto-interface.h"
#include "parse-options.h"
#include "run-command.h"

// To be filled in later see other builtin/*.c files for examples
static const char * const git_crypto_usage[] = {};

static int sign(int argc, const char **argv, const char *prefix)
{
    int ret_val = 0;
    const char *trusted_arg = NULL;
    const char *key_arg = NULL;
    const char *cert_arg = NULL;
    const char *commit_arg = NULL;
    // Options for the sign command
    struct option options[] = {
        OPT_STRING('t', "trusted", &trusted_arg, N_("trusted"),
                N_("Trusted list of certificates.")),
        OPT_STRING('c', "commit", &commit_arg, N_("commit"),
                N_("Commit to verify")),
        OPT_STRING('k', "key", &key_arg, N_("key"),
                N_("Signing key")),
        OPT_STRING('x', "cert", &key_arg, N_("certificate"),
                N_("Public certificate")),
        OPT_END()
    };
    argc = parse_options(argc, argv, prefix, options,
            git_crypto_usage, PARSE_OPT_STOP_AT_NON_OPTION);

    // Our Openssl variables for signing
    EVP_PKEY *key = NULL;
    X509 *cert = NULL;
    X509_STORE *stack = X509_STORE_new();

    if(!commit_arg){ // no commit arg so do all commits
        commit_arg = "HEAD";
    }

    if(!key_arg){ // no key argument so get it from the config
        key_arg = get_pem_path();
    }// use the file to get the  now
    key = get_key(key_arg);

    if(!cert_arg){ // no cert arg get it from config
        cert_arg = get_pem_path();
    }
    cert = get_cert(cert_arg);

    if(!trusted){ // get trusted list from config

    }

    ret_val = sign_commit_sha256(key, cert, stack, commit_arg);

    return ret_val;
}

void verify_err_helper(int err, char *commit){
    switch(err){
        case VERIFY_PASS:
            printf("%s: Verification Success\n", commit);
            break;
        case VERIFY_FAIL_NO_NOTE:
            printf("%s: Verification Warning, no signature for the commit.\n", commit);
            break;
        case VERIFY_FAIL_BAD_SIG:
            printf("%s: Verification Failure, invalid signature for the commit.\n", commit);
            break;
        case VERIFY_FAIL_NOT_TRUSTED:
            printf("%s: Verification Failure, untrusted signer for commit.\n", commit);
            break;
        case VERIFY_FAIL_COMPARE:
            printf("%s: Verification Failure, commit is inconsistent with signature.\n", commit);
            break;
    }
    return;
}

static int verify(int argc, const char **argv, const char *prefix)
{
    int ret_val = 0;
    const char *trusted_arg = NULL;
    char *commit_arg = NULL;
    // The list of our options
    struct option options[] = {
        OPT_STRING('t', "trusted", &trusted_arg, N_("trusted"),
                N_("Trusted list of certificates.")),
        OPT_STRING('c', "commit", &commit_arg, N_("commit"),
                N_("Commit to verify")),
        OPT_END()
    };
    argc = parse_options(argc, argv, prefix, options,
            git_crypto_usage, PARSE_OPT_STOP_AT_NON_OPTION);

    int verify_status = 0; // Return val of each call to verify

    if(!commit_arg){ // no commit arg so do all commits
        char **list = get_commit_list();
        for(char **commit = list; *commit != NULL; commit = commit + 1){
            verify_status = verify_commit(*commit);
            verify_err_helper(verify_status, *commit);
            ret_val = ret_val | verify_status;
        }
    }
    else { // verify only the specified commit
        verify_status = verify_commit(commit_arg);
        verify_err_helper(verify_status, commit_arg);
        ret_val = ret_val | verify_status;
    }

    if(ret_val == 0){
        printf("Verification SUCCESSFUL.\n");
    } else {
        printf("Verification FAILURE - error codes:%d\n", ret_val);
    }
    return ret_val;
}

int cmd_crypto(int argc, const char **argv, const char *prefix)
{
    int result;
    // TODO necessary?
    struct option options[] = {OPT_END()};
    argc = parse_options(argc, argv, prefix, options, NULL,
        PARSE_OPT_STOP_AT_NON_OPTION);


    if(!strcmp(argv[0], "sign"))
        result = sign(argc, argv, prefix);
    else if(!strcmp(argv[0], "verify"))
        result = verify(argc, argv, prefix);
    else
        printf("Unknown command for git crypto.\n");

    return result;
}

