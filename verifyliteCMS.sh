#!/bin/bash
#setup

NOTEID=$(git notes --ref=crypto | cut -d ' ' -f 1 | head -n1); 
COMMITID=$(git notes --ref=crypto | cut -d ' ' -f 2 | head -n1); 
TIME=$(date +\%s); 
git show $NOTEID > "$TIME".msg;
git show $COMMITID > "$TIME".cmt;
openssl smime -verify -in "$TIME".msg -noverify -signer "$TIME".pem -out "$TIME".data 2> /dev/null;
echo "Verifying the signature: ..." ;
openssl cms -verify -in "$TIME".msg -noverify -signer "$TIME".pem -out "$TIME".data;
tail -n +3 "$TIME".data > "$TIME"2.data;
if diff -w --brief <(sort "$TIME".cmt) <(sort "$TIME"2.data) > /dev/null ; then 
    echo "Verification passed for commit: $COMMITID"; 
else 
    echo "Verification FAILS for commit: $COMMITID"; 
fi; 
rm "$TIME"*
