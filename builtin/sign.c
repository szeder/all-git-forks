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
    
    //prepare script to grab commit, sign, and add to notes
    char * bash_script =
        "HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1) \
        openssl smime -sign -signer mycert.pem -in \".git/objects/${HASH:0:2}/${HASH:2}\" > cryptoSig.txt \
        git notes --ref=crypto add -F cryptoSig.txt HEAD \
        rm cryptoSig.txt \
        echo \"Pushing signed note to the origin\" \
        git push origin refs/notes/crypto/*";
    
    //execute commit using prepared commit
    int bashResult = system(bash_script);
    
    if(bashResult == BASH_ERROR)
        prinf("Error fetching signature, signing, adding to notes/n");
    
    
}
