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
#include <openssl/ssl.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/pkcs7.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#include <openssl/cms.h>

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
    
    //sign the commit
    sign_commit();
    
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

 int sign_commit() {
    BIO * in = NULL;
    X509 * cert = NULL;
    EVP_PKEY * key = NULL;
    int ret = 1;
    CMS_ContentInfo * cms = NULL;
    
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    
    //get the path of the .pem file containing the private key
    char * pem = get_config_val("user.certificate", '\0');
    
    //get the char * of the commit
    char ** commit_list = get_commit_list();
    
    //trim the trailing whitespace
    char * end;
    end = pem + strlen(pem) -1;
    while(end > pem && isspace(*end)) end--;
    //write new null terminator
    *(end+1) = 0;
    
    //read in .pem file
    in = BIO_new_file(pem,"r");
    
    //check for failure
    if(!in)
        goto err;
    
    //setting X509 * cert from BIO which read from pem
    cert = PEM_read_bio_X509(in, NULL, 0, NULL);
    
    BIO_reset(in);
    
    //read EVP_KEY * key from BIO which read from pem
    key = PEM_read_bio_PrivateKey(in, NULL, 0, NULL);
    
    //make sure these read in successfully
    if(!cert || !key)
        goto err;
    
    char calc_hash[65];
    
    //TODO not sure whether to use commit_list or commit_list2
    char * commit_list2 = &commit_list;
    printf("COMMIT LIST: %s\n", commit_list2);
    
    //create a txt file to hold the commit path
    FILE *file;
    file = fopen("commit_list.txt","w");
    fprintf(file,"%s", commit_list2);
    fclose(file);
    
    //SHA2 on the char* that contains the commit path
    calc_sha256("commit_list.txt", calc_hash); //prev msg.txt
    
    //remove the txt file
    
    printf("SHA2 hash: %s\n ", calc_hash);
    
    //put the hash into a BIO *
    BIO * data = BIO_new(BIO_s_mem());
    BIO_puts(data, calc_hash);
     
    //check for failure
    if(!data)
        goto err;
    
    //sign the message
    cms = CMS_sign(cert /*the certificate from .pem*/
                   ,key /*the private key from .pem*/
                   ,NULL /*stack of x509 certs, unneeded*/
                   ,data /*the data to be signed, aka sha2 hash of commit*/
                   ,CMS_DETACHED); /* flag for cleartext signing */

     //check for failure
     if(!cms)
         goto err;

     
    //attempt to verify for debugging
     X509_STORE *x509_st = X509_STORE_new();
     
     // Verify the s/smime message
     int err = CMS_verify(cms
                          , NULL /*stack x509*/
                          , x509_st
                          , data /*indata*/
                          , NULL /*out bio not used*/
                          , CMS_NO_SIGNER_CERT_VERIFY);
    
     //print whether or not verify was successful
     printf("Verify successful %d\n", err);
    
    ret = 0;
    
err:
    if(ret)
    {
        fprintf(stderr, "Error Signing Data\n");
        ERR_print_errors_fp(stderr);
    }
    
    if(cert)
        X509_free(cert);
    if(key)
        EVP_PKEY_free(key);
    if(data)
        BIO_free(data);
    if(in)
        BIO_free(in);
    
    return ret;

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

