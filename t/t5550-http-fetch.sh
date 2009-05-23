#!/bin/sh

test_description='test fetching over http'
. ./test-lib.sh

if test -n "$NO_CURL"; then
	say 'skipping test, git built without http support'
	test_done
fi

. "$TEST_DIRECTORY"/lib-httpd.sh
LIB_HTTPD_PORT=${LIB_HTTPD_PORT-'5550'}
start_httpd

test_expect_success 'setup repository' '
	echo content >file &&
	git add file &&
	git commit -m one
'

test_expect_failure 'try alternative URLs like local does' '
	mkdir "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	(cd "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	 git --bare init &&
	 git --bare update-server-info
	) &&

	#for http://foo/, try http://foo.git/
	git clone $HTTPD_URL/repo clone &&
	rm -rf clone &&

	#and http://foo/.git/
	mkdir "$HTTPD_DOCUMENT_ROOT_PATH/repo" &&
	mkdir "$HTTPD_DOCUMENT_ROOT_PATH/repo/.git" &&
	mv "$HTTPD_DOCUMENT_ROOT_PATH"/repo.git/* \
	 "$HTTPD_DOCUMENT_ROOT_PATH/repo/.git/" &&
	rm -rf "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	git clone $HTTPD_URL/repo clone &&
	rm -rf clone &&

	#for http://foo.git/, try neither http://foo.git/.git/
	mkdir "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	mv "$HTTPD_DOCUMENT_ROOT_PATH"/repo/.git \
	 "$HTTPD_DOCUMENT_ROOT_PATH/repo.git/" &&
	rm -rf "$HTTPD_DOCUMENT_ROOT_PATH/repo" &&
	! git clone $HTTPD_URL/repo.git clone &&

	#nor http://foo.git.git/
	mv "$HTTPD_DOCUMENT_ROOT_PATH"/repo.git/.git \
	 "$HTTPD_DOCUMENT_ROOT_PATH/repo.git.git/" &&
	rm -rf "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	! git clone $HTTPD_URL/repo.git clone &&

	rm -rf "$HTTPD_DOCUMENT_ROOT_PATH/repo.git.git"
'

test_expect_success 'create http-accessible bare repository' '
	if [ -d "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" ]; then
	 rm -rf "$HTTPD_DOCUMENT_ROOT_PATH/repo.git"
	fi
	mkdir "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	(cd "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	 git --bare init &&
	 echo "exec git update-server-info" >hooks/post-update &&
	 chmod +x hooks/post-update
	) &&
	git remote add public "$HTTPD_DOCUMENT_ROOT_PATH/repo.git" &&
	git push public master:master
'

test_expect_success 'clone http repository' '
	git clone $HTTPD_URL/repo.git clone &&
	test_cmp file clone/file
'

test_expect_success 'fetch changes via http' '
	echo content >>file &&
	git commit -a -m two &&
	git push public
	(cd clone && git pull) &&
	test_cmp file clone/file
'

test_expect_success 'http remote detects correct HEAD' '
	git push public master:other &&
	(cd clone &&
	 git remote set-head origin -d &&
	 git remote set-head origin -a &&
	 git symbolic-ref refs/remotes/origin/HEAD > output &&
	 echo refs/remotes/origin/master > expect &&
	 test_cmp expect output
	)
'

test_expect_success 'fetch packed objects' '
	cp -R "$HTTPD_DOCUMENT_ROOT_PATH"/repo.git "$HTTPD_DOCUMENT_ROOT_PATH"/repo_pack.git &&
	cd "$HTTPD_DOCUMENT_ROOT_PATH"/repo_pack.git &&
	git --bare repack &&
	git --bare prune-packed &&
	git clone $HTTPD_URL/repo_pack.git
'

stop_httpd
test_done
