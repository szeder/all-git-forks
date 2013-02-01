//
//  sign.c
//
//
//  Created by Vincent Clasgens on 1/30/13.
//  University of Portland (Team Rogue)
//
//
#include <stdio.h>
#define BASH_ERROR -1

int cmd_sign(int argc, const char **argv, const char *prefix)
{

    //execute commit using prepared commit
    int bashResult = system("./getAndSignCommit.sh");

    if(bashResult == BASH_ERROR)
        prinf("Error fetching signature, signing, adding to notes/n");
}
