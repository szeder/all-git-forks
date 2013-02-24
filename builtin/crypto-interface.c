//
//  sign.c
//
//
//  Created by Vincent Clasgens on 1/30/13.
//  Modified by Dustin Dalen on 2/4/13.
//  University of Portland (Team Rogue)
//
//
#include <stdio.h>
#include "crypto-interface.h"
#include "builtin.h"
#include "run-command.h"
#include "parse-options.h"

// To be filled in later see other builtin/*.c files for examples
static const char * const git_crypto_usage[] = {};


static int sign(const char *ref)
{
    // Waiting to convert this to C to pass in our ref arg
    //crypto_sign_buffer();
    printf("Dummy C sign handler in builtin/crypto-interface\n");
    return 0;
}

static int verify(int argc, const char **argv, const char *prefix)
{
    printf("Dummy C verify handler in builtin/crypto-interface");
    // Waiting to conver the verify to C before passing our args
    //crypto_verify_signed_buffer();
    return 0;
}

int cmd_crypto(int argc, const char **argv, const char *prefix)
{
    int result;
    const char *sign_arg = NULL;
    const char *verify_arg = NULL;

    // The list of our options which and instance variable
    // crypto-interface.c
    struct option options[] = {
        OPT_GROUP(N_("Crypto actions")),
        OPT_STRING('s', "sign", &sign_arg,
            N_("Sign"), N_("Sign the HEAD commit.")),
        OPT_STRING('v', "verify", &verify_arg,
            N_("Verify"), N_("Verify the HEAD commit.")),
        OPT_END()
    };

    argc = parse_options(argc, argv, prefix, options,
            git_crypto_usage, PARSE_OPT_STOP_AT_NON_OPTION);

    if(!strcmp(argv[1], "sign") || sign_arg)
        result = sign(sign_arg);
    else if(verify_arg)
        result = verify(argc, argv, prefix);
    return result;
}
