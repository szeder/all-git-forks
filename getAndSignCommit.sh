#!/bin/bash
#NOTE: you must have a cert name mycert.pem in ./.
HASH=$(git log -n1 | cut -d ' ' -f 2 | head -n1)
FILE=$(date +%s).txt
git show $(HASH) > "$FILE"
cat "$FILE"
openssl cms -sign -in "$FILE" -text -out "$FILE" -signer myCert.pem
git notes --ref=crypto add -F "$FILE" HEAD
rm "$FILE"
echo "Pushing signed note to the origin"
git push origin refs/notes/crypto/*
