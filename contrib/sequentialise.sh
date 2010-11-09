#!/bin/bash
set -e

DIGITS=${2-5}

TEMP=$(mktemp)
trap "rm $TEMP" EXIT

EDITOR='sed -i s/pick/edit/' git rebase -i $1 2>$TEMP
head -n1 $TEMP

while \
	git commit --amend --set-commit-id $(perl -e 'printf("%0'$DIGITS'x0",0x'$(git rev-list HEAD~ | head -c$DIGITS)'+1)') -CHEAD && \
	git rebase --continue 2>$TEMP
	do head -n1 $TEMP
done

