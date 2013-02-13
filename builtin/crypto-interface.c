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
#include "strbuf.h"
#include "parse-options.h"

int cmd_crypto(int argc, const char **argv, const char *prefix)
{
    // The list of our options which reference methods in
    // /crypto-interface.c
    static struct option builtin_crypto_options[] = {
        OPT_GROUP(N_("Crypto actions")),
        OPT_STRING('s', "sign", &crypto_sign_buffer,
            N_("Sign"), N_("Sign the HEAD commit.")),
        OPT_STRING('v', "verify", &crypto_verify_signed_buffer,
            N_("Verify"), N_("Verify the HEAD commit.")),
        OPT_END()
    };

    // TEMPORARY
    if(strcmp(argv[1], "-s") == 0){
        char * pem = malloc(sizeof(argv[2]));
        strcpy(pem, argv[2]);
        printf("Sign \n");
        crypto_sign_buffer(pem);
    }
    if(strcmp(argv[1], "--verify") == 0){
        char * pem = malloc(sizeof(argv[2]));
        strcpy(pem, argv[2]);
        crypto_verify_signed_buffer(pem);
    }
}
