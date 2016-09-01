#!/bin/sh

test_description='tests for git prime-clone'
. ./test-lib.sh

ROOT_PATH="$PWD"
. "$TEST_DIRECTORY"/lib-httpd.sh
start_httpd

test_expect_success 'resume fails for no parameters' '
	test_must_fail git clone --resume
'

test_expect_success 'resume fails with other options' '
	test_must_fail git clone --resume --bare
'

test_expect_success 'resume fails for excess parameters' '
	test_must_fail git clone --resume a b
'

test_expect_success 'resume fails for nonexistent directory' '
	test_must_fail git clone --resume nonexistent
'

test_expect_success 'setup repo and httpd' '
	mkdir server &&
	cd server &&
	git init &&
	echo "content\\n" >example.c &&
	git add example.c &&
	git commit -m "I am a packed commit" &&
	git repack . &&
	git config --local http.primeclone true &&
	git config --local primeclone.url \
		$HTTPD_URL/server/.git/objects/pack/$(find .git/objects/pack/*.pack -printf "%f") &&
	git config --local primeclone.filetype pack &&
	echo "content\\n" >example2.c &&
	echo "new content\\n" >example.c &&
	git add example.c example2.c &&
	git commit -m "I am an unpacked commit" &&
	cd - &&
	mv server "$HTTPD_DOCUMENT_ROOT_PATH"
'

test_expect_success 'prime-clone works http' '
	git clone $HTTPD_URL/smart/server client &&
	rm -r client
'

test_expect_success 'prime-clone falls back not permitted' '
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	git config --local http.primeclone false &&
	cd - &&
	git clone $HTTPD_URL/smart/server client &&
	rm -r client &&
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	git config --local http.primeclone true &&
	cd -
'

test_expect_success 'prime-clone falls back not enabled' '
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	git config --local primeclone.enabled 0 &&
	cd - &&
	git clone $HTTPD_URL/smart/server client &&
	rm -r client &&
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	git config --local --unset-all primeclone.enabled &&
	cd -
'

test_expect_success 'prime-clone falls back incorrect config' '
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	git config --local --unset-all primeclone.filetype &&
	cd - &&
	git clone $HTTPD_URL/smart/server client &&
	rm -r client &&
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	git config --local primeclone.filetype pack &&
	cd -
'

test_expect_success 'clone resume fails in complete/unmarked directory' '
	git clone $HTTPD_URL/smart/server client &&
	test_must_fail git clone --resume client &&
	rm -r client
'

test_expect_success 'clone resume works with marked repo (work dir, normal)' '
	git clone $HTTPD_URL/smart/server client &&
	cd client &&
	rm .git/objects/pack/*.idx &&
	echo -n "$HTTPD_URL/server/" > .git/RESUMABLE &&
	find .git/objects/pack/*.pack >> .git/RESUMABLE &&
	echo "pack" >> .git/RESUMABLE &&
	mv $(find .git/objects/pack/*.pack) $(find .git/objects/pack/*.pack).tmp &&
	sed -i "2,$ d" $(find .git/objects/pack/*.pack.tmp) &&
	rm * &&
	git clone --resume . &&
	cd - &&
	rm -r client
'

test_expect_success 'clone resume works with marked repo (git dir, normal)' '
	git clone $HTTPD_URL/smart/server client &&
	cd client &&
	rm .git/objects/pack/*.idx &&
	echo -n "$HTTPD_URL/server/" > .git/RESUMABLE &&
	find .git/objects/pack/*.pack >> .git/RESUMABLE &&
	echo "pack" >> .git/RESUMABLE &&
	mv $(find .git/objects/pack/*.pack) $(find .git/objects/pack/*.pack).tmp &&
	sed -i "2,$ d" $(find .git/objects/pack/*.pack.tmp) &&
	rm * &&
	git clone --resume .git &&
	cd - &&
	rm -r client
'

test_expect_success 'clone resume works inside marked repo (git dir, normal)' '
	git clone $HTTPD_URL/smart/server client &&
	cd client &&
	rm .git/objects/pack/*.idx &&
	echo -n "$HTTPD_URL/server/" > .git/RESUMABLE &&
	find .git/objects/pack/*.pack >> .git/RESUMABLE &&
	echo "pack" >> .git/RESUMABLE &&
	mv $(find .git/objects/pack/*.pack) $(find .git/objects/pack/*.pack).tmp &&
	sed -i "2,$ d" $(find .git/objects/pack/*.pack.tmp) &&
	rm * &&
	cd .git &&
	git clone --resume . &&
	cd ../.. &&
	rm -r client
'

test_expect_success 'clone resume works with marked repo (work dir, split)' '
	git clone --separate-git-dir=separate_dir.git \
		$HTTPD_URL/smart/server client &&
	cd separate_dir.git &&
	rm objects/pack/*.idx &&
	echo -n "$HTTPD_URL/server/" > RESUMABLE &&
	echo ".git/$(find objects/pack/*.pack)" >> RESUMABLE &&
	echo "pack" >> RESUMABLE &&
	mv $(find objects/pack/*.pack) $(find objects/pack/*.pack).tmp &&
	sed -i "2,$ d" $(find objects/pack/*.pack.tmp) &&
	cd ../client &&
	rm * &&
	cd .. &&
	git clone --resume client &&
	rm -r client separate_dir.git
'

test_expect_success 'clone resume works with marked repo (git dir, split)' '
	git clone --separate-git-dir=separate_dir.git \
		$HTTPD_URL/smart/server client &&
	cd separate_dir.git &&
	rm objects/pack/*.idx &&
	echo -n "$HTTPD_URL/server/" > RESUMABLE &&
	echo ".git/$(find objects/pack/*.pack)" >> RESUMABLE &&
	echo "pack" >> RESUMABLE &&
	mv $(find objects/pack/*.pack) $(find objects/pack/*.pack).tmp &&
	sed -i "2,$ d" $(find objects/pack/*.pack.tmp) &&
	cd ../client &&
	rm * &&
	cd .. &&
	git clone --resume separate_dir.git &&
	rm -r client separate_dir.git
'

test_expect_success 'prime-clone falls back unusable file' '
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	git config --local primeclone.url $HTTPD_URL/server/.git/HEAD &&
	cd - &&
	git clone $HTTPD_URL/smart/server client &&
	rm -r client &&
	cd "$HTTPD_DOCUMENT_ROOT_PATH/server" &&
	cd -
'

stop_httpd
test_done
