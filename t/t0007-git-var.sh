#!/bin/sh

test_description='basic sanity checks for git var'
. ./test-lib.sh

test_expect_success 'get GIT_AUTHOR_IDENT' '
	test_tick &&
	echo "A U Thor <author@example.com> 1112911993 -0700" >expect &&
	git var GIT_AUTHOR_IDENT >actual &&
	test_cmp expect actual
'

test_expect_success 'get GIT_COMMITTER_IDENT' '
	test_tick &&
	echo "C O Mitter <committer@example.com> 1112912053 -0700" >expect &&
	git var GIT_COMMITTER_IDENT >actual &&
	test_cmp expect actual
'

test_expect_success 'git var can show multiple values' '
	cat >expect <<-\EOF &&
	A U Thor <author@example.com> 1112912053 -0700
	C O Mitter <committer@example.com> 1112912053 -0700
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
