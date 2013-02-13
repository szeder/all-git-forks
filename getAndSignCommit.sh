#!/bin/bash
#NOTE: you must have a cert name mycert.pem in ./.
HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1)
FILE=$(date +%s)
git show $(HASH) > "$FILE".txt
openssl cms -sign -in "$FILE".txt -text -out "$FILE".msg -signer myCert.pem
git notes --ref=crypto add -F "$FILE".msg HEAD
rm "$FILE" "$FILE".msg
echo "Pushing signed note to the origin"
git push origin refs/notes/crypto/*
