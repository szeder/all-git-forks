#!/bin/sh

test_expect_success 'set up terminal for tests' '
	if
		test_have_prereq PERL &&
		"$PERL_PATH" "$TEST_DIRECTORY"/test-terminal.perl \
			sh -c "test -t 1 && test -t 2"
	then
		test_set_prereq TTY &&
		test_terminal () {
			if ! test_declared_prereq TTY
			then
				echo >&4 "test_terminal: need to declare TTY prerequisite"
				return 127
			fi
			"$PERL_PATH" "$TEST_DIRECTORY"/test-terminal.perl "$@"
		}
	fi
'

cat >expect1 <<EOF
stdin: 1
stdout: 1
stderr: 1
EOF
: >expect2

test_expect_success TTY 'test-terminal.perl is sane' '
	test_terminal perl -e "
		use POSIX qw(isatty);
		print \"stdin: \", isatty(STDIN), \"\\n\";
		print \"stdout: \", isatty(STDOUT), \"\\n\";
		print \"stderr: \", isatty(STDERR), \"\\n\";
	" >actual1 &&
	test_cmp expect1 actual1 &&
	echo foo | test_terminal cat - >actual2 &&
	test_cmp expect2 actual2
'
