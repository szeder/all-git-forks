#!/bin/sh

test_description='help'

. ./test-lib.sh

test_expect_success "works for commands and guides by default" "
	git help status &&
	git help revisions
"

test_expect_success "--command-only does not work for guides" "
	git help --command-only status &&
	cat <<-EOF >expected &&
		git: 'revisions' is not a git command. See 'git --help'.
	EOF
	(git help --command-only revisions 2>actual || true) &&
	test_i18ncmp expected actual
"

test_done
