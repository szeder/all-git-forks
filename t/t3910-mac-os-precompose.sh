#!/bin/sh
#
# Copyright (c) 2012 Torsten Bögershausen
#

test_description='utf-8 decomposed (nfd) converted to precomposed (nfc)'

. ./test-lib.sh

Adiarnfc=`printf '\303\204'`
Odiarnfc=`printf '\303\226'`
Adiarnfd=`printf 'A\314\210'`
Odiarnfd=`printf 'O\314\210'`

# check if the feature is compiled in
mkdir junk &&
>junk/"$Adiarnfc" &&
case "$(cd junk && echo *)" in
	"$Adiarnfd")
	test_nfd=1
	;;
	*)	;;
esac
rm -rf junk


if test "$test_nfd"
then
	test_expect_success "detect if nfd needed" '
		precomposedunicode=`git config core.precomposedunicode` &&
		test "$precomposedunicode" = false &&
		git config core.precomposedunicode true
	'
	test_expect_success "setup" '
		>x &&
		git add x &&
		git commit -m "1st commit" &&
		git rm x &&
		git commit -m "rm x"
	'
	test_expect_success "setup case mac" '
		git checkout -b mac_os
	'
	# This will test nfd2nfc in readdir()
	test_expect_success "add file Adiarnfc" '
		echo f.Adiarnfc >f.$Adiarnfc &&
		git add f.$Adiarnfc &&
		git commit -m "add f.$Adiarnfc"
	'
	# This will test nfd2nfc in git stage()
	test_expect_success "stage file d.Adiarnfd/f.Adiarnfd" '
		mkdir d.$Adiarnfd &&
		echo d.$Adiarnfd/f.$Adiarnfd >d.$Adiarnfd/f.$Adiarnfd &&
		git stage d.$Adiarnfd/f.$Adiarnfd &&
		git commit -m "add d.$Adiarnfd/f.$Adiarnfd"
	'
	test_expect_success "add link Adiarnfc" '
		ln -s d.$Adiarnfd/f.$Adiarnfd l.$Adiarnfc &&
		git add l.$Adiarnfc &&
		git commit -m "add l.Adiarnfc"
	'
	# This will test git log
	test_expect_success "git log f.Adiar" '
		git log f.$Adiarnfc > f.Adiarnfc.log &&
		git log f.$Adiarnfd > f.Adiarnfd.log &&
		test -s f.Adiarnfc.log &&
		test -s f.Adiarnfd.log &&
		test_cmp f.Adiarnfc.log f.Adiarnfd.log &&
		rm f.Adiarnfc.log f.Adiarnfd.log
	'
	# This will test git ls-files
	test_expect_success "git lsfiles f.Adiar" '
		git ls-files f.$Adiarnfc > f.Adiarnfc.log &&
		git ls-files f.$Adiarnfd > f.Adiarnfd.log &&
		test -s f.Adiarnfc.log &&
		test -s f.Adiarnfd.log &&
		test_cmp f.Adiarnfc.log f.Adiarnfd.log &&
		rm f.Adiarnfc.log f.Adiarnfd.log
	'
	# This will test git mv
	test_expect_success "git mv" '
		git mv f.$Adiarnfd f.$Odiarnfc &&
		git mv d.$Adiarnfd d.$Odiarnfc &&
		git mv l.$Adiarnfd l.$Odiarnfc &&
		git commit -m "mv Adiarnfd Odiarnfc"
	'
	# Files can be checked out as nfc
	# And the link has been corrected from nfd to nfc
	test_expect_success "git checkout nfc" '
		rm f.$Odiarnfc &&
		git checkout f.$Odiarnfc
	'
	# Make it possible to checkout files with their NFD names
	test_expect_success "git checkout file nfd" '
		rm -f f.* &&
		git checkout f.$Odiarnfd
	'
	# Make it possible to checkout links with their NFD names
	test_expect_success "git checkout link nfd" '
		rm l.* &&
		git checkout l.$Odiarnfd
	'
	test_expect_success "setup case mac2" '
		git checkout master &&
		git reset --hard &&
		git checkout -b mac_os_2
	'
	# This will test nfd2nfc in git commit
	test_expect_success "commit file d2.Adiarnfd/f.Adiarnfd" '
		mkdir d2.$Adiarnfd &&
		echo d2.$Adiarnfd/f.$Adiarnfd >d2.$Adiarnfd/f.$Adiarnfd &&
		git add d2.$Adiarnfd/f.$Adiarnfd &&
		git commit -m "add d2.$Adiarnfd/f.$Adiarnfd" -- d2.$Adiarnfd/f.$Adiarnfd
	'
	# Test if the global core.precomposedunicode stops autosensing
	# Must be the last test case
	test_expect_success "respect git config --global core.precomposedunicode" '
		git config --global core.precomposedunicode true &&
		rm -rf .git &&
		git init &&
		precomposedunicode=`git config core.precomposedunicode` &&
		test "$precomposedunicode" = "true"
	'
else
	 say "Skipping nfc/nfd tests"
fi

test_done
