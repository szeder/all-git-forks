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

	mkdir generated-test-data &&
	for i in $(test_seq 1 $T0021_LARGE_FILE_SIZE)
	do
		RANDOM_STRING="$(test-genrandom end $i | tr -dc "A-Za-z0-9" )"
		ROT_RANDOM_STRING="$(echo $RANDOM_STRING | ./rot13.sh )"
		# Generate 1MB of empty data and 100 bytes of random characters
		# printf "$(test-genrandom start $i)"
		printf "%1048576d" 1 >>generated-test-data/large.file &&
		printf "$RANDOM_STRING" >>generated-test-data/large.file &&
		printf "%1048576d" 1 >>generated-test-data/large.file.rot13 &&
		printf "$ROT_RANDOM_STRING" >>generated-test-data/large.file.rot13 &&

		if test $i = $T0021_LARGISH_FILE_SIZE
		then
			cat generated-test-data/large.file >generated-test-data/largish.file &&
			cat generated-test-data/large.file.rot13 >generated-test-data/largish.file.rot13
		fi
	done
'

script='s/^\$Id: \([0-9a-f]*\) \$/\1/p'

test_expect_success check '

	test_cmp test.o test &&
	test_cmp test.o test.t &&

	# ident should be stripped in the repository
	git diff --raw --exit-code :test :test.i &&
	id=$(git rev-parse --verify :test) &&
	embedded=$(sed -ne "$script" test.i) &&
	test "z$id" = "z$embedded" &&

	git cat-file blob :test.t >test.r &&

	./rot13.sh <test.o >test.t &&
	test_cmp test.r test.t
'

# If an expanded ident ever gets into the repository, we want to make sure that
# it is collapsed before being expanded again on checkout
test_expect_success expanded_in_repo '
	{
		echo "File with expanded keywords"
		echo "\$Id\$"
		echo "\$Id:\$"
		echo "\$Id: 0000000000000000000000000000000000000000 \$"
		echo "\$Id: NoSpaceAtEnd\$"
		echo "\$Id:NoSpaceAtFront \$"
		echo "\$Id:NoSpaceAtEitherEnd\$"
		echo "\$Id: NoTerminatingSymbol"
		echo "\$Id: Foreign Commit With Spaces \$"
	} >expanded-keywords.0 &&

	{
		cat expanded-keywords.0 &&
		printf "\$Id: NoTerminatingSymbolAtEOF"
	} >expanded-keywords &&
	cat expanded-keywords >expanded-keywords-crlf &&
	git add expanded-keywords expanded-keywords-crlf &&
	git commit -m "File with keywords expanded" &&
	id=$(git rev-parse --verify :expanded-keywords) &&

	{
		echo "File with expanded keywords"
		echo "\$Id: $id \$"
		echo "\$Id: $id \$"
		echo "\$Id: $id \$"
		echo "\$Id: $id \$"
		echo "\$Id: $id \$"
		echo "\$Id: $id \$"
		echo "\$Id: NoTerminatingSymbol"
		echo "\$Id: Foreign Commit With Spaces \$"
	} >expected-output.0 &&
	{
		cat expected-output.0 &&
		printf "\$Id: NoTerminatingSymbolAtEOF"
	} >expected-output &&
	{
		append_cr <expected-output.0 &&
		printf "\$Id: NoTerminatingSymbolAtEOF"
	} >expected-output-crlf &&
	{
		echo "expanded-keywords ident"
		echo "expanded-keywords-crlf ident text eol=crlf"
	} >>.gitattributes &&

	rm -f expanded-keywords expanded-keywords-crlf &&

	git checkout -- expanded-keywords &&
	test_cmp expanded-keywords expected-output &&

	git checkout -- expanded-keywords-crlf &&
	test_cmp expanded-keywords-crlf expected-output-crlf
'

# The use of %f in a filter definition is expanded to the path to
# the filename being smudged or cleaned.  It must be shell escaped.
# First, set up some interesting file names and pet them in
# .gitattributes.
test_expect_success 'filter shell-escaped filenames' '
	cat >argc.sh <<-EOF &&
	#!$SHELL_PATH
	cat >/dev/null
	echo argc: \$# "\$@"
	EOF
	normal=name-no-magic &&
	special="name  with '\''sq'\'' and \$x" &&
	echo some test text >"$normal" &&
	echo some test text >"$special" &&
	git add "$normal" "$special" &&
	git commit -q -m "add files" &&
	echo "name* filter=argc" >.gitattributes &&

	# delete the files and check them out again, using a smudge filter
	# that will count the args and echo the command-line back to us
	test_config filter.argc.smudge "sh ./argc.sh %f" &&
	rm "$normal" "$special" &&
	git checkout -- "$normal" "$special" &&

	# make sure argc.sh counted the right number of args
	echo "argc: 1 $normal" >expect &&
	test_cmp expect "$normal" &&
	echo "argc: 1 $special" >expect &&
	test_cmp expect "$special" &&

	# do the same thing, but with more args in the filter expression
	test_config filter.argc.smudge "sh ./argc.sh %f --my-extra-arg" &&
	rm "$normal" "$special" &&
	git checkout -- "$normal" "$special" &&

	# make sure argc.sh counted the right number of args
	echo "argc: 2 $normal --my-extra-arg" >expect &&
	test_cmp expect "$normal" &&
	echo "argc: 2 $special --my-extra-arg" >expect &&
	test_cmp expect "$special" &&
	:
'

test_expect_success 'required filter should filter data' '
	test_config filter.required.smudge ./rot13.sh &&
	test_config filter.required.clean ./rot13.sh &&
	test_config filter.required.required true &&

	echo "*.r filter=required" >.gitattributes &&

	cat test.o >test.r &&
	git add test.r &&

	rm -f test.r &&
	git checkout -- test.r &&
	test_cmp test.o test.r &&

	./rot13.sh <test.o >expected &&
	git cat-file blob :test.r >actual &&
	test_cmp expected actual
'

test_expect_success 'required filter smudge failure' '
	test_config filter.failsmudge.smudge false &&
	test_config filter.failsmudge.clean cat &&
	test_config filter.failsmudge.required true &&

	echo "*.fs filter=failsmudge" >.gitattributes &&

	echo test >test.fs &&
	git add test.fs &&
	rm -f test.fs &&
	test_must_fail git checkout -- test.fs
'

test_expect_success 'required filter clean failure' '
	test_config filter.failclean.smudge cat &&
	test_config filter.failclean.clean false &&
	test_config filter.failclean.required true &&

	echo "*.fc filter=failclean" >.gitattributes &&

	echo test >test.fc &&
	test_must_fail git add test.fc
'

test_expect_success 'filtering large input to small output should use little memory' '
	test_config filter.devnull.clean "cat >/dev/null" &&
	test_config filter.devnull.required true &&
	cp generated-test-data/large.file large.file &&
	echo "large.file filter=devnull" >.gitattributes &&
	GIT_MMAP_LIMIT=1m GIT_ALLOC_LIMIT=1m git add large.file
'

test_expect_success 'filter that does not read is fine' '
	test-genrandom foo $((128 * 1024 + 1)) >big &&
	echo "big filter=epipe" >.gitattributes &&
	test_config filter.epipe.clean "echo xyzzy" &&
	git add big &&
	git cat-file blob :big >actual &&
	echo xyzzy >expect &&
	test_cmp expect actual
'

test_expect_success 'filter large file' '
	test_config filter.largefile.smudge cat &&
	test_config filter.largefile.clean cat &&
	echo "large.file filter=largefile" >.gitattributes &&
	cp generated-test-data/large.file large.file &&
	git add large.file 2>err &&
	test_must_be_empty err &&
	rm -f large.file &&
	git checkout -- large.file 2>err &&
	test_must_be_empty err
'

test_expect_success "filter: clean empty file" '
	test_config filter.in-repo-header.clean  "echo cleaned && cat" &&
	test_config filter.in-repo-header.smudge "sed 1d" &&

	echo "empty-in-worktree    filter=in-repo-header" >>.gitattributes &&
	>empty-in-worktree &&

	echo cleaned >expected &&
	git add empty-in-worktree &&
	git show :empty-in-worktree >actual &&
	test_cmp expected actual
'

test_expect_success "filter: smudge empty file" '
	test_config filter.empty-in-repo.clean "cat >/dev/null" &&
	test_config filter.empty-in-repo.smudge "echo smudged && cat" &&

	echo "empty-in-repo filter=empty-in-repo" >>.gitattributes &&
	echo dead data walking >empty-in-repo &&
	git add empty-in-repo &&

	echo smudged >expected &&
	git checkout-index --prefix=filtered- empty-in-repo &&
	test_cmp expected filtered-empty-in-repo
'

test_expect_success 'disable filter with empty override' '
	test_config_global filter.disable.smudge false &&
	test_config_global filter.disable.clean false &&
	test_config filter.disable.smudge false &&
	test_config filter.disable.clean false &&

	echo "*.disable filter=disable" >.gitattributes &&

	echo test >test.disable &&
	git -c filter.disable.clean= add test.disable 2>err &&
	test_must_be_empty err &&
	rm -f test.disable &&
	git -c filter.disable.smudge= checkout -- test.disable 2>err &&
	test_must_be_empty err
'

check_filter () {
	rm -f rot13-filter.log actual.log &&
	"$@" 2> git_stderr.log &&
	test_must_be_empty git_stderr.log &&
	cat >expected.log &&
	sort rot13-filter.log | uniq -c | sed "s/^[ ]*//" >actual.log &&
	test_cmp expected.log actual.log
}

check_filter_count_clean () {
	rm -f rot13-filter.log actual.log &&
	"$@" 2> git_stderr.log &&
	test_must_be_empty git_stderr.log &&
	cat >expected.log &&
	sort rot13-filter.log | uniq -c | sed "s/^[ ]*//" |
		sed "s/^\([0-9]\) IN: clean/x IN: clean/" >actual.log &&
	test_cmp expected.log actual.log
}

check_filter_ignore_clean () {
	rm -f rot13-filter.log actual.log &&
	"$@" &&
	cat >expected.log &&
	grep -v "IN: clean" rot13-filter.log >actual.log &&
	test_cmp expected.log actual.log
}

check_filter_no_call () {
	rm -f rot13-filter.log &&
	"$@" 2> git_stderr.log &&
	test_must_be_empty git_stderr.log &&
	test_must_be_empty rot13-filter.log
}

check_rot13 () {
	test_cmp $1 $2 &&
	./../rot13.sh <$1 >expected &&
	git cat-file blob :$2 >actual &&
	test_cmp expected actual
}

test_expect_success PERL 'required process filter should filter data' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean smudge" &&
	test_config_global filter.protocol.required true &&
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

		check_filter \
			git add . \
				<<-\EOF &&
					1 IN: clean test.r 57 [OK] -- OUT: 57 [OK]
					1 IN: clean test2.r 14 [OK] -- OUT: 14 [OK]
					1 IN: clean test4-empty.r 0 [OK] -- OUT: 0 [OK]
					1 IN: clean testsubdir/test3-subdir.r 21 [OK] -- OUT: 21 [OK]
					1 start
					1 wrote filter header
				EOF

		check_filter_count_clean \
			git commit . -m "test commit" \
				<<-\EOF &&
					x IN: clean test.r 57 [OK] -- OUT: 57 [OK]
					x IN: clean test2.r 14 [OK] -- OUT: 14 [OK]
					x IN: clean test4-empty.r 0 [OK] -- OUT: 0 [OK]
					x IN: clean testsubdir/test3-subdir.r 21 [OK] -- OUT: 21 [OK]
					1 start
					1 wrote filter header
				EOF

		rm -f test?.r testsubdir/test3-subdir.r &&

		check_filter_ignore_clean \
			git checkout . \
				<<-\EOF &&
					start
					wrote filter header
					IN: smudge test2.r 14 [OK] -- OUT: 14 [OK]
					IN: smudge testsubdir/test3-subdir.r 21 [OK] -- OUT: 21 [OK]
				EOF

		check_filter_ignore_clean \
			git checkout empty \
				<<-\EOF &&
					start
					wrote filter header
				EOF

		check_filter_ignore_clean \
			git checkout master \
				<<-\EOF &&
					start
					wrote filter header
					IN: smudge test.r 57 [OK] -- OUT: 57 [OK]
					IN: smudge test2.r 14 [OK] -- OUT: 14 [OK]
					IN: smudge test4-empty.r 0 [OK] -- OUT: 0 [OK]
					IN: smudge testsubdir/test3-subdir.r 21 [OK] -- OUT: 21 [OK]
				EOF

		check_rot13 ../test.o test.r &&
		check_rot13 ../test2.o test2.r &&
		check_rot13 ../test3-subdir.o testsubdir/test3-subdir.r
	)
'

test_expect_success PERL 'required process filter should filter smudge data and one-shot filter should clean' '
	test_config_global filter.protocol.clean ./../rot13.sh &&
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl smudge" &&
	test_config_global filter.protocol.required true &&
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

		check_filter_no_call \
			git add . &&

		check_filter_no_call \
			git commit . -m "test commit" &&

		rm -f test?.r testsubdir/test3-subdir.r &&

		check_filter_ignore_clean \
			git checkout . \
				<<-\EOF &&
					start
					wrote filter header
					IN: smudge test2.r 14 [OK] -- OUT: 14 [OK]
				EOF

		git checkout empty &&

		check_filter_ignore_clean \
			git checkout master\
				<<-\EOF &&
					start
					wrote filter header
					IN: smudge test.r 57 [OK] -- OUT: 57 [OK]
					IN: smudge test2.r 14 [OK] -- OUT: 14 [OK]
				EOF

		check_rot13 ../test.o test.r &&
		check_rot13 ../test2.o test2.r
	)
'

test_expect_success PERL 'required process filter should clean only' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean" &&
	test_config_global filter.protocol.required true &&
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

		check_filter \
			git add . \
				<<-\EOF &&
					1 IN: clean test.r 57 [OK] -- OUT: 57 [OK]
					1 start
					1 wrote filter header
				EOF

		check_filter_count_clean \
			git commit . -m "test commit" \
				<<-\EOF
					x IN: clean test.r 57 [OK] -- OUT: 57 [OK]
					1 start
					1 wrote filter header
				EOF
	)
'

test_expect_success PERL 'required process filter should process binary files larger LARGE_PACKET_MAX' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean smudge" &&
	test_config_global filter.protocol.required true &&
	rm -rf repo &&
	mkdir repo &&
	(
		cd repo &&
		git init &&

		echo "*.file filter=protocol" >.gitattributes &&
		cat ../generated-test-data/largish.file.rot13 >large.rot13 &&
		cat ../generated-test-data/largish.file >large.file &&
		cat large.file >large.original &&

		git add large.file .gitattributes &&
		git commit . -m "test commit" &&

		rm -f large.file &&
		git checkout -- large.file &&
		git cat-file blob :large.file >actual &&
		test_cmp large.rot13 actual
	)
'

test_expect_success PERL 'required process filter should with clean error should fail' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean smudge" &&
	test_config_global filter.protocol.required true &&
	rm -rf repo &&
	mkdir repo &&
	(
		cd repo &&
		git init &&

		echo "*.r filter=protocol" >.gitattributes &&

		cp ../test.o test.r &&
		echo "this is going to fail" >clean-write-fail.r &&
		echo "content-test3-subdir" >test3.r &&

		# Note: There are three clean paths in convert.c we just test one here.
		test_must_fail git add .
	)
'

test_expect_success PERL 'process filter should restart after unexpected write failure' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean smudge" &&
	rm -rf repo &&
	mkdir repo &&
	(
		cd repo &&
		git init &&

		echo "*.r filter=protocol" >.gitattributes &&

		cp ../test.o test.r &&
		cp ../test2.o test2.r &&
		echo "this is going to fail" >smudge-write-fail.o &&
		cat smudge-write-fail.o >smudge-write-fail.r &&
		git add . &&
		git commit . -m "test commit" &&
		rm -f *.r &&

		check_filter_ignore_clean \
			git checkout . \
				<<-\EOF &&
					start
					wrote filter header
					IN: smudge smudge-write-fail.r 22 [OK] -- OUT: 22 [WRITE FAIL]
					start
					wrote filter header
					IN: smudge test.r 57 [OK] -- OUT: 57 [OK]
					IN: smudge test2.r 14 [OK] -- OUT: 14 [OK]
				EOF

		check_rot13 ../test.o test.r &&
		check_rot13 ../test2.o test2.r &&

		! test_cmp smudge-write-fail.o smudge-write-fail.r && # Smudge failed!
		./../rot13.sh <smudge-write-fail.o >expected &&
		git cat-file blob :smudge-write-fail.r >actual &&
		test_cmp expected actual							  # Clean worked!
	)
'

test_expect_success PERL 'process filter should not restart in case of an error' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean smudge" &&
	rm -rf repo &&
	mkdir repo &&
	(
		cd repo &&
		git init &&

		echo "*.r filter=protocol" >.gitattributes &&

		cp ../test.o test.r &&
		cp ../test2.o test2.r &&
		echo "this will cause an error" >error.o &&
		cp error.o error.r &&
		git add . &&
		git commit . -m "test commit" &&
		rm -f *.r &&

		check_filter_ignore_clean \
			git checkout . \
				<<-\EOF &&
					start
					wrote filter header
					IN: smudge error.r 25 [OK] -- OUT: 0 [ERROR]
					IN: smudge test.r 57 [OK] -- OUT: 57 [OK]
					IN: smudge test2.r 14 [OK] -- OUT: 14 [OK]
				EOF

		check_rot13 ../test.o test.r &&
		check_rot13 ../test2.o test2.r &&
		test_cmp error.o error.r
	)
'

test_expect_success PERL 'process filter should be able to signal an error for all future files' '
	test_config_global filter.protocol.process "$TEST_DIRECTORY/t0021/rot13-filter.pl clean smudge" &&
	rm -rf repo &&
	mkdir repo &&
	(
		cd repo &&
		git init &&

		echo "*.r filter=protocol" >.gitattributes &&

		cp ../test.o test.r &&
		cp ../test2.o test2.r &&
		echo "error this blob and all future blobs" >error-all.o &&
		cp error-all.o error-all.r &&
		git add . &&
		git commit . -m "test commit" &&
		rm -f *.r &&

		check_filter_ignore_clean \
			git checkout . \
				<<-\EOF &&
					start
					wrote filter header
					IN: smudge error-all.r 37 [OK] -- OUT: 0 [ERROR-ALL]
				EOF

		test_cmp ../test.o test.r &&
		test_cmp ../test2.o test2.r &&
		test_cmp error-all.o error-all.r
	)
'

test_expect_success PERL 'invalid process filter must fail (and not hang!)' '
	test_config_global filter.protocol.process cat &&
	test_config_global filter.protocol.required true &&
	rm -rf repo &&
	mkdir repo &&
	(
		cd repo &&
		git init &&

		echo "*.r filter=protocol" >.gitattributes &&

		cp ../test.o test.r &&
		test_must_fail git add . 2> git_stderr.log &&
		grep "not support long running filter protocol" git_stderr.log
	)
'

test_done
