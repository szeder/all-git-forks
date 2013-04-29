#!/bin/bash
#NOTE: you must have a cert name mycert.pem in ~/.
HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1);
FILE=$(date +\%s);
git cat-file -p "$HASH" > "$FILE".txt;
openssl dgst -sha256 "$FILE".txt | cut -d ' ' -f 2 > "$FILE".sha
openssl cms -sign -in "$FILE".sha -text -out "$FILE".msg -signer ~/myCert.pem;
git notes --ref=crypto add -F "$FILE".msg HEAD;
rm "$FILE".txt "$FILE".msg "$FILE".sha;
echo "Successfully signed: $HASH";
