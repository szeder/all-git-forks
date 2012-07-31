#!/bin/sh

test_description='messages from rebase operation'

. ./test-lib.sh

quick_one () {
	fileno=$2
	test -z "$fileno" && fileno=$1
	echo "$1" >"file$fileno" &&
	git add "file$fileno" &&
	test_tick &&
	git commit -m "$1"
}

test_expect_success setup '
	quick_one O &&
	git branch topic &&
	quick_one X &&
	quick_one A &&
	quick_one B A &&
	quick_one Y &&

	git checkout topic &&
	quick_one A &&
	quick_one B A &&
	quick_one Z &&
	git tag start

'

cat >expect <<\EOF
Committed: 0001 Z
EOF

test_expect_success 'rebase -m' '

	git rebase -m master >report &&
	sed -n -e "/^Already applied: /p" \
		-e "/^Committed: /p" report >actual &&
	test_cmp expect actual

'

test_expect_success 'rebase --stat' '
	git reset --hard start &&
        git rebase --stat master >diffstat.txt &&
        grep "^ fileX |  *1 +$" diffstat.txt
'

test_expect_success 'rebase w/config rebase.stat' '
	git reset --hard start &&
        git config rebase.stat true &&
        git rebase master >diffstat.txt &&
        grep "^ fileX |  *1 +$" diffstat.txt
'

test_expect_success 'rebase -n overrides config rebase.stat config' '
	git reset --hard start &&
        git config rebase.stat true &&
        git rebase -n master >diffstat.txt &&
        ! grep "^ fileX |  *1 +$" diffstat.txt
'

# Output to stderr:
#
#     "Does not point to a valid commit: invalid-ref"
#
# NEEDSWORK: This "grep" is fine in real non-C locales, but
# GETTEXT_POISON poisons the refname along with the enclosing
# error message.
test_expect_success 'rebase --onto outputs the invalid ref' '
	test_must_fail git rebase --onto invalid-ref HEAD HEAD 2>err &&
	test_i18ngrep "invalid-ref" err
'

test_done
