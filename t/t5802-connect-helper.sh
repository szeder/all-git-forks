#!/bin/sh

test_description='ext::cmd remote "connect" helper'
. ./test-lib.sh

escape_spaces () {
	echo "$*" | sed -e "s/ /% /g"
}

test_expect_success setup '
	git config --global protocol.ext.allow user &&
	test_tick &&
	git commit --allow-empty -m initial &&
	test_tick &&
	git commit --allow-empty -m second &&
	test_tick &&
	git commit --allow-empty -m third &&
	test_tick &&
	git tag -a -m "tip three" three &&

	test_tick &&
	git commit --allow-empty -m fourth
'

test_expect_success clone '
	cmd=$(escape_spaces "echo >&2 ext::sh invoked && %S ..") &&
	git clone "ext::sh -c %S% ." dst &&
	git for-each-ref refs/heads/ refs/tags/ >expect &&
	(
		cd dst &&
		git config remote.origin.url "ext::sh -c $cmd" &&
		git for-each-ref refs/heads/ refs/tags/
	) >actual &&
	test_cmp expect actual
'

test_expect_success 'update following tag' '
	test_tick &&
	git commit --allow-empty -m fifth &&
	test_tick &&
	git tag -a -m "tip five" five &&
	git for-each-ref refs/heads/ refs/tags/ >expect &&
	(
		cd dst &&
		git pull &&
		git for-each-ref refs/heads/ refs/tags/ >../actual
	) &&
	test_cmp expect actual
'

test_expect_success 'update backfilled tag' '
	test_tick &&
	git commit --allow-empty -m sixth &&
	test_tick &&
	git tag -a -m "tip two" two three^1 &&
	git for-each-ref refs/heads/ refs/tags/ >expect &&
	(
		cd dst &&
		git pull &&
		git for-each-ref refs/heads/ refs/tags/ >../actual
	) &&
	test_cmp expect actual
'

test_expect_success 'update backfilled tag without primary transfer' '
	test_tick &&
	git tag -a -m "tip one " one two^1 &&
	git for-each-ref refs/heads/ refs/tags/ >expect &&
	(
		cd dst &&
		git pull &&
		git for-each-ref refs/heads/ refs/tags/ >../actual
	) &&
	test_cmp expect actual
'

test_expect_success 'GIT_EXT_SERVICE for clone, ls-remote, push, archive' '
	rm -rf dst &&
	>actual &&
	cat >expect <<-\EOF &&
	git-upload-pack
	git-upload-pack
	git-receive-pack
	git-upload-archive
	EOF
	git archive HEAD >expect.tar &&
	cmd=$(escape_spaces "echo >>actual \$GIT_EXT_SERVICE && %S .") &&

	git clone "ext::sh -c $cmd" dst &&
	git ls-remote "ext::sh -c $cmd" &&
	test_when_finished "git update-ref -d refs/heads/newbranch" &&
	git push "ext::sh -c $cmd" master:newbranch &&
	git archive --remote="ext::sh -c $cmd" HEAD >actual.tar &&

	test_cmp expect actual &&
	test_cmp expect.tar actual.tar
'

test_expect_success 'GIT_EXT_SERVICE_NOPREFIX for clone, ls-remote, push, archive' '
	rm -rf dst &&
	>actual &&
	cat >expect <<-\EOF &&
	upload-pack
	upload-pack
	receive-pack
	upload-archive
	EOF
	git archive HEAD >expect.tar &&
	cmd=$(escape_spaces "echo >>actual \$GIT_EXT_SERVICE_NOPREFIX && %S .") &&

	git clone "ext::sh -c $cmd" dst &&
	git ls-remote "ext::sh -c $cmd" &&
	test_when_finished "git update-ref -d refs/heads/newbranch" &&
	git push "ext::sh -c $cmd" master:newbranch &&
	git archive --remote="ext::sh -c $cmd" HEAD >actual.tar &&

	test_cmp expect actual &&
	test_cmp expect.tar actual.tar
'

test_expect_success 'GIT_DIR is set to the enclosing repo for ls-remote' '
	git rev-parse --git-dir >expect &&
	cmd=$(escape_spaces "echo \$GIT_DIR >actual && %S .") &&
	git ls-remote "ext::sh -c $cmd" &&
	test_cmp expect actual
'

test_expect_success 'GIT_DIR is set to the enclosing repo for archive' '
	git rev-parse --git-dir >expect &&
	git archive HEAD >expect.tar &&
	cmd=$(escape_spaces "echo \$GIT_DIR >actual && %S .") &&
	git archive --remote="ext::sh -c $cmd" HEAD >actual.tar &&
	test_cmp expect actual &&
	test_cmp expect.tar actual.tar
'

test_expect_success 'GIT_DIR is not set if there is no enclosing repo' '
	rm -rf subdir &&
	>actual &&
	printf "%s\n" unset unset >expect &&
	git archive HEAD >expect.tar &&

	mkdir subdir &&
	cmd=$(escape_spaces "echo \${GIT_DIR-unset} >>../actual && %S ..") &&
	(
		GIT_CEILING_DIRECTORIES=$(pwd) &&
		export GIT_CEILING_DIRECTORIES &&
		cd subdir &&
		git ls-remote "ext::sh -c $cmd" &&
		git archive --remote="ext::sh -c $cmd" HEAD >../actual.tar
	) &&
	test_cmp expect actual &&
	test_cmp expect.tar actual.tar
'

test_expect_success 'set up fake git-daemon' '
	mkdir remote &&
	git init --bare remote/one.git &&
	mkdir remote/host &&
	git init --bare remote/host/two.git &&
	write_script fake-daemon <<-\EOF &&
	git daemon --inetd \
		--informative-errors \
		--export-all \
		--base-path="$TRASH_DIRECTORY/remote" \
		--interpolated-path="$TRASH_DIRECTORY/remote/%H%D" \
		"$TRASH_DIRECTORY/remote"
	EOF
	export TRASH_DIRECTORY &&
	PATH=$TRASH_DIRECTORY:$PATH
'

test_expect_success 'ext command can connect to git daemon (no vhost)' '
	rm -rf dst &&
	git clone "ext::fake-daemon %G/one.git" dst
'

test_expect_success 'ext command can connect to git daemon (vhost)' '
	rm -rf dst &&
	git clone "ext::fake-daemon %G/two.git %Vhost" dst
'

test_done
