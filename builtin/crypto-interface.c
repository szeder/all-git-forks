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
#include "builtin.h"
#include "builtin/config.h"
#include "commit.h"
#include "crypto-interface.h"
#include "parse-options.h"
#include "run-command.h"


// To be filled in later see other builtin/*.c files for examples
static const char * const git_crypto_usage[] = {};

static int parse_sign_arg(const struct option *opt, const char *arg, int unset)
{
    // the arg is in opt->value
    // we don't need to do anything to it this is a place holder
    return 0;
}

static int parse_verify_arg(const struct option *opt, const char *arg, int unset)
{
    return 0;
}

static int parse_trusted_arg(const struct option *opt, const char *arg, int unset)
{
    return 0;
}

static int parse_key_arg(const struct option *opt, const char *arg, int unset)
{
    return 0;
}

static int parse_commit_arg(const struct option *opt, const char *arg, int unset)
{
    return 0;
}

static int sign(int argc, const char **argv, const char *prefix)
{
    const char *trusted_arg = NULL;
    const char *key_arg = NULL;
    const char *commit_arg = NULL;
    // The list of our options
    struct option options[] = {
        { OPTION_CALLBACK, 't', "trusted", &trusted_arg,
            N_("Trusted"), N_("Trusted list."), PARSE_OPT_NONEG,
            parse_trusted_arg},
        { OPTION_CALLBACK, 'k', "key", &key_arg,
            N_("Key"), N_("Signing Key."), PARSE_OPT_NONEG,
            parse_key_arg},
        { OPTION_CALLBACK, 'c', "commit", &commit_arg,
            N_("Commit"), N_("Commit to verify."), PARSE_OPT_NONEG,
            parse_commit_arg},
        OPT_END()
    };
    argc = parse_options(argc, argv, prefix, options,
            git_crypto_usage, PARSE_OPT_STOP_AT_NON_OPTION);



// LEGACY CODE - TODO delme

    //*********NOTES********//
//    unsigned char object[20], new_note[20];
//    const char * object_ref;
//    const unsigned char * note;
//    struct msg_arg msg = {  0, 0, STRBUF_INIT };
//
//    create_note(object,&msg,0,note,new_note);
    //*********NOTES********//

/*
    char * return_val2 = get_config_val("user.certificate",'\0');

    // Waiting to convert this to C to pass in our ref arg
    printf("Dummy C sign handler in builtin/crypto-interface\n");
    return 0;
    */
}

static int verify(int argc, const char **argv, const char *prefix)
{
    const char *trusted_arg = NULL;
    const char *commit_arg = NULL;
    // The list of our options
    struct option options[] = {
        { OPTION_CALLBACK, 't', "trusted", &trusted_arg,
            N_("Trusted"), N_("Trusted list."), PARSE_OPT_NONEG,
            parse_trusted_arg},
        { OPTION_CALLBACK, 'c', "commit", &commit_arg,
            N_("Commit"), N_("Commit to verify."), PARSE_OPT_NONEG,
            parse_commit_arg},
        OPT_END()
    };
    argc = parse_options(argc, argv, prefix, options,
            git_crypto_usage, PARSE_OPT_STOP_AT_NON_OPTION);

    return 0;
}

int cmd_crypto(int argc, const char **argv, const char *prefix)
{
    int result;
    struct option options[] = {OPT_END()};
    argc = parse_options(argc, argv, prefix, options, NULL,
        PARSE_OPT_STOP_AT_NON_OPTION);

    // DEBUG here to force testing
    //printf("\n\nreturn: \n %d\n", verify_commit("31789b477bbc2ecd3f92d3cee9234a91baaf8590"));

    if(!strcmp(argv[0], "sign"))
        result = sign(argc, argv, prefix);
    else if(!strcmp(argv[0], "verify"))
        result = verify(argc, argv, prefix);
    else
        printf("Unknown command for git crypto.\n");

    return result;
}

