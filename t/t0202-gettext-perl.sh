#!/bin/sh

test_description='Perl gettext interface (Git::I18N)'
. ./test-lib.sh

GIT_TEXTDOMAINDIR="$GIT_EXEC_PATH/share/locale"
export GIT_TEXTDOMAINDIR

if ! test_have_prereq PERL; then
	say 'skipping perl interface tests, perl not available'
	test_done
fi

"$PERL_PATH" -MTest::More -e 0 2>/dev/null || {
	say "Perl Test::More unavailable, skipping test"
	test_done
}

test_external_without_stderr \
    'Perl Git::I18N API' \
    "$PERL_PATH" "$TEST_DIRECTORY"/t0202/test.pl

test_done
