#!/bin/sh

test_description='upload-pack ref-in-want'

. ./test-lib.sh

line() {
	len=$(printf '%s' "$1" | wc -c) &&
	len=$((len + 5)) &&
	printf '%04x%s\n' $len "$1" >>input
}

flush() {
	printf '0000'
}

get_actual_refs() {
	grep -a wanted-ref output | sed "s/^.*wanted-ref //" >actual_refs
}

get_actual_commits() {
	perl -0777 -p -e "s/^.*PACK/PACK/s" <output >o.pack &&
	git index-pack o.pack &&
	git verify-pack -v o.idx | grep commit | cut -c-40 | sort >actual_commits
}

check_output() {
	get_actual_refs &&
	test_cmp expected_refs actual_refs &&
	get_actual_commits &&
	test_cmp expected_commits actual_commits
}

# c(o/foo) d(o/bar)
#        \ /
#         b   e(baz)  f(master)
#          \__  |  __/
#             \ | /
#               a
test_expect_success 'setup repository' '
	test_commit a &&
	git checkout -b o/foo &&
	test_commit b &&
	test_commit c &&
	git checkout -b o/bar b &&
	test_commit d &&
	git checkout -b baz a &&
	test_commit e &&
	git checkout master &&
	test_commit f
'

test_expect_success 'config controls ref-in-want advertisement' '
	test_config uploadpack.advertiseRefInWant false &&
	printf "0000" | git upload-pack . >output &&
	! grep -a ref-in-want output &&
	test_config uploadpack.advertiseRefInWant true &&
	printf "0000" | git upload-pack . >output &&
	grep -a ref-in-want output
'

test_expect_success 'mix want and want-ref' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse f) refs/heads/master
	EOF
	git rev-parse e f | sort >expected_commits &&

	line "want-ref refs/heads/master" >input &&
	line "want $(git rev-parse e)" >>input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'want-ref with glob and non-glob' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse f) refs/heads/master
	$(git rev-parse d) refs/heads/o/bar
	$(git rev-parse c) refs/heads/o/foo
	EOF
	git rev-parse b c d f | sort >expected_commits &&

	line "want-ref refs/head*/[op]/*" >input &&
	line "want-ref refs/heads/master" >>input &&
	line "want-ref refs/heads/non-existent/*" >>input &&
	line "want-ref refs/heads/also-non-existent" >>input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'want-ref with SHA-1' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse f) $(git rev-parse f)
	EOF
	git rev-parse f | sort >expected_commits &&

	line "want-ref $(git rev-parse f)" >input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'want-ref with overlapping glob and non-glob' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse d) refs/heads/o/bar
	$(git rev-parse c) refs/heads/o/foo
	EOF
	git rev-parse b c d | sort >expected_commits &&

	line "want-ref refs/heads/o/*" >input &&
	line "want-ref refs/heads/o/foo" >>input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'want-ref with overlapping non-glob and SHA-1' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse f) $(git rev-parse f)
	$(git rev-parse f) refs/heads/master
	EOF
	git rev-parse f | sort >expected_commits &&

	line "want-ref $(git rev-parse f)" >input &&
	line "want-ref refs/heads/master" >>input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'want-ref with overlapping glob and SHA-1' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse f) $(git rev-parse f)
	$(git rev-parse f) refs/heads/master
	EOF
	git rev-parse f | sort >expected_commits &&

	line "want-ref $(git rev-parse f)" >input &&
	line "want-ref refs/heads/mas*" >>input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'want-ref with overlapping globs' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse d) refs/heads/o/bar
	$(git rev-parse c) refs/heads/o/foo
	EOF
	git rev-parse b c d | sort >expected_commits &&

	line "want-ref refs/heads/o/*" >input &&
	line "want-ref refs/heads/o/f*" >>input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'want-ref with ref we already have commit for' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse c) refs/heads/o/foo
	EOF
	>expected_commits &&

	line "want-ref refs/heads/o/foo" >input &&
	flush >>input &&
	line "have $(git rev-parse c)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'send wanted-ref only at the end of negotiation' '
	# Incomplete input; acknowledge "have" with NAK, but no wanted-ref
	line "want-ref refs/heads/o/foo" >input &&
	flush >>input &&
	line "have 1234567890123456789012345678901234567890" >>input &&
	flush >>input &&
	test_must_fail git upload-pack . <input >output &&
	grep -a "0008NAK" output &&
	test_must_fail grep -a "wanted-ref" output &&

	# Complete the input, and try again
	line "have $(git rev-parse c)" >>input &&
	flush >>input &&
	line "done" >>input &&
	flush >>input &&
	git upload-pack . <input >output &&
	grep -a "wanted-ref" output
'

test_expect_success 'want-ref with capability declaration' '
	cat >expected_refs <<-EOF &&
	$(git rev-parse c) refs/heads/o/foo
	EOF
	git rev-parse b c | sort >expected_commits &&

	line "want-ref refs/heads/o/foo no_progress" >input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'hideRefs' '
	test_config transfer.hideRefs refs/heads/o/foo &&

	cat >expected_refs <<-EOF &&
	$(git rev-parse d) refs/heads/o/bar
	EOF
	git rev-parse b d | sort >expected_commits &&

	line "want-ref refs/heads/o/*" >input &&
	flush >>input &&
	line "have $(git rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git upload-pack . <input >output &&
	check_output
'

test_expect_success 'setup namespaced repo' '
	git init n &&
	(
		cd n &&
		test_commit a &&
		test_commit b &&
		git checkout a &&
		test_commit c &&
		git checkout a &&
		test_commit d &&
		git update-ref refs/heads/ns-no b
		git update-ref refs/namespaces/ns/refs/heads/ns-yes c
		git update-ref refs/namespaces/ns/refs/heads/another d
	)
'

test_expect_success 'want-ref with namespaces' '
	cat >expected_refs <<-EOF &&
	$(git -C n rev-parse c) refs/heads/ns-yes
	EOF
	git -C n rev-parse c | sort >expected_commits &&

	line "want-ref refs/heads/ns-*" >input &&
	flush >>input &&
	line "have $(git -C n rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git --namespace=ns -C n upload-pack . <input >output &&
	check_output
'

test_expect_success 'hideRefs with namespaces' '
	test_config transfer.hideRefs refs/heads/another &&

	cat >expected_refs <<-EOF &&
	$(git -C n rev-parse c) refs/heads/ns-yes
	EOF
	git -C n rev-parse c | sort >expected_commits &&

	line "want-ref refs/heads/ns-*" >input &&
	flush >>input &&
	line "have $(git -C n rev-parse a)" >>input &&
	flush >>input &&
	line "done" >>input &&
	git --namespace=ns -C n upload-pack . <input >output &&
	check_output
'

. "$TEST_DIRECTORY"/lib-httpd.sh
start_httpd

REPO="$HTTPD_DOCUMENT_ROOT_PATH/repo"
LOCAL_PRISTINE="$(pwd)/local_pristine"

test_expect_success 'setup repos for change-while-negotiating test' '
	(
		git init "$REPO" &&
		cd "$REPO" &&
		>.git/git-daemon-export-ok &&
		test_commit m1 &&
		git tag -d m1 &&

		# Local repo with many commits (so that negotiation will take
		# more than 1 request/response pair)
		git clone "http://127.0.0.1:$LIB_HTTPD_PORT/smart/repo" "$LOCAL_PRISTINE" &&
		cd "$LOCAL_PRISTINE" &&
		git checkout -b side &&
		for i in $(seq 1 33); do test_commit s$i; done &&

		# Add novel commits to upstream
		git checkout master &&
		cd "$REPO" &&
		test_commit m2 &&
		test_commit m3 &&
		git tag -d m2 m3
	) &&
	git -C "$LOCAL_PRISTINE" remote set-url origin "http://127.0.0.1:$LIB_HTTPD_PORT/one_time_sed/repo"
'

inconsistency() {
	# Simulate that the server initially reports $2 as the ref
	# corresponding to $1, and after that, $1 as the ref corresponding to
	# $1. This corresponds to the real-life situation where the server's
	# repository appears to change during negotiation, for example, when
	# different servers in a load-balancing arrangement serve (stateless)
	# RPCs during a single negotiation.
	printf "s/%s/%s/" \
	       $(git -C "$REPO" rev-parse $1 | tr -d "\n") \
	       $(git -C "$REPO" rev-parse $2 | tr -d "\n") \
	       >"$HTTPD_ROOT_PATH/one-time-sed"
}

test_expect_success 'server is initially ahead - no ref in want' '
	git -C "$REPO" config uploadpack.advertiseRefInWant false &&
	rm -rf local &&
	cp -r "$LOCAL_PRISTINE" local &&
	inconsistency master 1234567890123456789012345678901234567890 &&
	test_must_fail git -C local fetch 2> err &&
	grep "ERR upload-pack: not our ref" err
'

test_expect_success 'server is initially ahead - ref in want' '
	git -C "$REPO" config uploadpack.advertiseRefInWant true &&
	rm -rf local &&
	cp -r "$LOCAL_PRISTINE" local &&
	inconsistency master 1234567890123456789012345678901234567890 &&
	git -C local fetch &&

	git -C "$REPO" rev-parse --verify master > expected &&
	git -C local rev-parse --verify refs/remotes/origin/master > actual &&
	test_cmp expected actual
'

test_expect_success 'server is initially behind - no ref in want' '
	git -C "$REPO" config uploadpack.advertiseRefInWant false &&
	rm -rf local &&
	cp -r "$LOCAL_PRISTINE" local &&
	inconsistency master "master^" &&
	git -C local fetch &&

	git -C "$REPO" rev-parse --verify "master^" > expected &&
	git -C local rev-parse --verify refs/remotes/origin/master > actual &&
	test_cmp expected actual
'

test_expect_success 'server is initially behind - ref in want' '
	git -C "$REPO" config uploadpack.advertiseRefInWant true &&
	rm -rf local &&
	cp -r "$LOCAL_PRISTINE" local &&
	inconsistency master "master^" &&
	git -C local fetch &&

	git -C "$REPO" rev-parse --verify "master" > expected &&
	git -C local rev-parse --verify refs/remotes/origin/master > actual &&
	test_cmp expected actual
'

stop_httpd

test_done
