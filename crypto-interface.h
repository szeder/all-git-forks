/**
 *  File: crypto-interface.h
 *  Created by: Team Rogue (University of Portland)
 *                  - Vince Clasgens
 *                  - Dustin Dalen
 *                  - David Garcia
 *                  - Sam Chase
 *  Last Modified: April 2, 2013
 *  
 *  Functions defined to sign and verify using 'git crypto' 
 *  command using CMS library within OpenSSL
 *
 *
 **/

#ifndef CYPTO_INTERFACE_H
#define CYPTO_INTERFACE_H

//VERIFYING RETURN CODES
#define VERIFY_PASS             0
#define VERIFY_FAIL_NO_NOTE     1
#define VERIFY_FAIL_BAD_SIG     2
#define VERIFY_FAIL_NOT_TRUSTED 4
#define VERIFY_FAIL_COMPARE     8

/**
 *  get_commit_list()
 *  
 *  Parameters: none
 *  
 *  Retrieves an array of char* which are the SHA's of the refs of each commit
 *      - with the last item being NULL 
 * 
 **/
extern char ** get_commit_list();

/**
 *  free_cmt_list()
 *
 *  Parameters: char** list
 *      - Array of char* which are SHA's of the refs of each commit
 *      - Represents the commit list
 *
 *  Free's each char* in the commit list
 *
 *
 **/
extern void free_cmt_list(char**);

/**
 *  verify_commit()
 *
 *  Paramaters: char * sha1
 *      - SHA1 ref of commit to be verified
 *  
 *  Verifies the given SHA1 Ref of a commit and the note if one is present
 *
 **/
extern int verify_commit(char * sha1);

/**
 *  sign_commit_sha()
 * 
 *  Parameters: char * sha
 *      - SHA1 ref of commit to be signed
 *  
 *  Signs a commit given a SHA1 ref of a commit 
 *
 *
 **/
extern int sign_commit_sha(char * sha);

#endif
