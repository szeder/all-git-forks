//
//  sign.c
//
//
//  Created by Vincent Clasgens on 1/30/13.
//
//

#include <stdio.h>

int cmd_sign(int argc, const char **argv, const char *prefix)
{
    char * bash_script =
        "HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1) \
        openssl smime -sign -signer mycert.pem -in \".git/objects/${HASH:0:2}/${HASH:2}\" > cryptoSig.txt \
        git notes --ref=crypto add -F cryptoSig.txt HEAD \
        rm cryptoSig.txt \
        echo \"Pushing signed note to the origin\" \
        git push origin refs/notes/crypto/*";
    system(bash_script);
}
