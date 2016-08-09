#!/bin/sh

test_description='blob conversion via gitattributes'

. ./test-lib.sh

if test_have_prereq EXPENSIVE
then
	T0021_LARGE_FILE_SIZE=2048
	T0021_LARGISH_FILE_SIZE=100
else
	T0021_LARGE_FILE_SIZE=30
	T0021_LARGISH_FILE_SIZE=2
fi

cat <<EOF >rot13.sh
#!$SHELL_PATH
tr \
  'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ' \
  'nopqrstuvwxyzabcdefghijklmNOPQRSTUVWXYZABCDEFGHIJKLM'
EOF
chmod +x rot13.sh

test_expect_success setup '
	test_config filter.rot13.smudge ./rot13.sh &&
	test_config filter.rot13.clean ./rot13.sh &&

	{
	    echo "*.t filter=rot13"
	    echo "*.i ident"
	} >.gitattributes &&

	{
	    echo a b c d e f g h i j k l m
	    echo n o p q r s t u v w x y z
	    echo '\''$Id$'\''
	} >test &&
	cat test >test.t &&
	cat test >test.o &&
	cat test >test.i &&
	git add test test.t test.i &&
	rm -f test test.t test.i &&
	git checkout -- test test.t test.i &&

	echo "content-test2" >test2.o &&
	echo "content-test3-subdir" >test3-subdir.o &&

	mkdir generated-test-data
'

script='s/^\$Id: \([0-9a-f]*\) \$/\1/p'

check_rot13 () {
	test_cmp $1 $2 &&
	./../rot13.sh <$1 >expected &&
	git cat-file blob :$2 >actual &&
	test_cmp expected actual
}

test_expect_success PERL 'required process filter should filter data' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean smudge" &&
	rm -rf repo &&
	mkdir repo &&
	(
		cd repo &&
		git init &&

		echo "*.r filter=protocol" >.gitattributes &&
		git add . &&
		git commit . -m "test commit" &&
		git branch empty &&

		cp ../test.o test.r &&
		cp ../test2.o test2.r &&
		mkdir testsubdir &&
		cp ../test3-subdir.o testsubdir/test3-subdir.r &&
		>test4-empty.r &&

		git add . &&

		git commit . -m "test commit" &&

		rm -f test?.r testsubdir/test3-subdir.r &&

		git checkout . &&

		export GIT_TRACE=4 &&

		git checkout empty &&

		git checkout master
	)
'


test_done
