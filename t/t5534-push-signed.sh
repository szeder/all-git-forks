#!/bin/sh

test_description='signed push'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-gpg.sh

prepare_dst () {
	rm -fr dst &&
	test_create_repo dst &&

	git push dst master:noop master:ff master:noff
}

test_expect_success setup '
	# master, ff and noff branches pointing at the same commit
	test_tick &&
	git commit --allow-empty -m initial &&

	git checkout -b noop &&
	git checkout -b ff &&
	git checkout -b noff &&

	# noop stays the same, ff advances, noff rewrites
	test_tick &&
	git commit --allow-empty --amend -m rewritten &&
	git checkout ff &&

	test_tick &&
	git commit --allow-empty -m second
'

test_expect_success 'unsigned push does not send push certificate' '
	prepare_dst &&
	mkdir -p dst/.git/hooks &&
	write_script dst/.git/hooks/post-receive <<-\EOF &&
	if test -n "${GIT_PUSH_CERT-}"
	then
		git cat-file blob $GIT_PUSH_CERT >../push-cert
	fi
	EOF

	git push dst noop ff +noff &&
	test -f dst/push-cert &&
	! test -s dst/push-cert
'

test_expect_success GPG 'signed push sends push certificate' '
	prepare_dst &&
	mkdir -p dst/.git/hooks &&
	write_script dst/.git/hooks/post-receive <<-\EOF &&
	if test -n "${GIT_PUSH_CERT-}"
	then
		git cat-file blob $GIT_PUSH_CERT >../push-cert
	fi &&

	cat >../push-cert-status <<E_O_F
	SIGNER=${GIT_PUSH_CERT_SIGNER-nobody}
	KEY=${GIT_PUSH_CERT_KEY-nokey}
	STATUS=${GIT_PUSH_CERT_STATUS-nostatus}
	E_O_F

	EOF

	cat >expect <<-\EOF &&
	SIGNER=C O Mitter <committer@example.com>
	KEY=13B6F51ECDDE430D
	STATUS=G
	EOF

	git push --signed dst noop ff +noff &&
	grep "$(git rev-parse noop ff) refs/heads/ff" dst/push-cert &&
	grep "$(git rev-parse noop noff) refs/heads/noff" dst/push-cert &&
	test_cmp expect dst/push-cert-status
'

test_done
