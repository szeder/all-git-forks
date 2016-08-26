#!/bin/sh

test_description='help'

. ./test-lib.sh

configure_help () {
	test_config help.format html &&
	test_config help.htmlpath test://html &&
	test_config help.browser firefox
}

test_expect_success "setup" "
	write_script firefox <<-\EOF
	exit 0
	EOF
"

test_expect_success "works for commands and guides by default" "
	configure_help &&
	git help status &&
	git help revisions
"

test_expect_success "--exclude-guides does not work for guides" "
	cat <<-EOF >expected &&
		git: 'revisions' is not a git command. See 'git --help'.
	EOF
	test_must_fail git help --exclude-guides revisions 2>actual &&
	test_i18ncmp expected actual
"

test_done
