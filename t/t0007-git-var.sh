#!/bin/sh

test_description='basic sanity checks for git var'
. ./test-lib.sh

test_expect_success 'get GIT_AUTHOR_IDENT' '
	test_tick &&
	echo "$GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE" >expect &&
	git var GIT_AUTHOR_IDENT >actual &&
	test_cmp expect actual
'

test_expect_success 'get GIT_COMMITTER_IDENT' '
	test_tick &&
	echo "$GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE" >expect &&
	git var GIT_COMMITTER_IDENT >actual &&
	test_cmp expect actual
'

# For git var -l, we check only a representative variable;
# testing the whole output would make our test too brittle with
# respect to unrelated changes in the test suite's environment.
test_expect_success 'git var -l lists variables' '
	git var -l >actual &&
	echo "$GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE" >expect &&
	sed -n s/GIT_AUTHOR_IDENT=//p <actual >actual.author &&
	test_cmp expect actual.author
'

test_expect_success 'git var -l lists config' '
	git var -l >actual &&
	echo false >expect &&
	sed -n s/core\\.bare=//p <actual >actual.bare &&
	test_cmp expect actual.bare
'

test_expect_success 'listing and asking for variables are exclusive' '
	test_must_fail git var -l GIT_COMMITTER_IDENT
'

test_expect_success 'git var can show multiple values' '
	cat >expect <<-EOF &&
	$GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	$GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	EOF
	git var GIT_AUTHOR_IDENT GIT_COMMITTER_IDENT >actual &&
	test_cmp expect actual
'

for type in AUTHOR COMMITTER; do
	test_expect_success "$type ident can detect implicit values" "
		echo 0 >expect &&
		(
			sane_unset GIT_${type}_NAME &&
			sane_unset GIT_${type}_EMAIL &&
			sane_unset EMAIL &&
			git var GIT_${type}_EXPLICIT >actual
		) &&
		test_cmp expect actual
	"

	test_expect_success "$type ident is explicit via environment" "
		echo 1 >expect &&
		(
			GIT_${type}_NAME='A Name' &&
			export GIT_${type}_NAME &&
			GIT_${type}_EMAIL='name@example.com' &&
			export GIT_${type}_EMAIL &&
			git var GIT_${type}_EXPLICIT >actual
		) &&
		test_cmp expect actual
	"

	test_expect_success "$type ident is explicit via config" "
		echo 1 >expect &&
		test_config user.name 'A Name' &&
		test_config user.email 'name@example.com' &&
		(
			sane_unset GIT_${type}_NAME &&
			sane_unset GIT_${type}_EMAIL &&
			git var GIT_${type}_EXPLICIT >actual
		)
	"
done

test_done
