#!/bin/sh

test_description='test for-each-refs usage of ref-filter APIs'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-gpg.sh

if ! test_have_prereq GPG
then
	skip_all="skipping for-each-ref tests, GPG not available"
	test_done
fi

test_expect_success 'setup some history and refs' '
	test_commit one &&
	test_commit two &&
	test_commit three &&
	git checkout -b side &&
	test_commit four &&
	git tag -s -m "A signed tag message" signed-tag &&
	git tag -s -m "Annonated doubly" double-tag signed-tag &&
	git checkout master &&
	git update-ref refs/odd/spot master
'

test_expect_success 'filtering with --points-at' '
	cat >expect <<-\EOF &&
	refs/heads/master
	refs/odd/spot
	refs/tags/three
	EOF
	git for-each-ref --format="%(refname)" --points-at=master >actual &&
	test_cmp expect actual
'

test_expect_success 'check signed tags with --points-at' '
	sed -e "s/Z$//" >expect <<-\EOF &&
	refs/heads/side Z
	refs/tags/four Z
	refs/tags/signed-tag four
	EOF
	git for-each-ref --format="%(refname) %(*subject)" --points-at=side >actual &&
	test_cmp expect actual
'

test_expect_success 'filtering with --merged' '
	cat >expect <<-\EOF &&
	refs/heads/master
	refs/odd/spot
	refs/tags/one
	refs/tags/three
	refs/tags/two
	EOF
	git for-each-ref --format="%(refname)" --merged=master >actual &&
	test_cmp expect actual
'

test_expect_success 'filtering with --no-merged' '
	cat >expect <<-\EOF &&
	refs/heads/side
	refs/tags/double-tag
	refs/tags/four
	refs/tags/signed-tag
	EOF
	git for-each-ref --format="%(refname)" --no-merged=master >actual &&
	test_cmp expect actual
'

test_expect_success 'filtering with --contains' '
	cat >expect <<-\EOF &&
	refs/heads/master
	refs/heads/side
	refs/odd/spot
	refs/tags/double-tag
	refs/tags/four
	refs/tags/signed-tag
	refs/tags/three
	refs/tags/two
	EOF
	git for-each-ref --format="%(refname)" --contains=two >actual &&
	test_cmp expect actual
'

test_expect_success 'left alignment' '
	cat >expect <<-\EOF &&
	refname is refs/heads/master  |refs/heads/master
	refname is refs/heads/side    |refs/heads/side
	refname is refs/odd/spot      |refs/odd/spot
	refname is refs/tags/double-tag|refs/tags/double-tag
	refname is refs/tags/four     |refs/tags/four
	refname is refs/tags/one      |refs/tags/one
	refname is refs/tags/signed-tag|refs/tags/signed-tag
	refname is refs/tags/three    |refs/tags/three
	refname is refs/tags/two      |refs/tags/two
	EOF
	git for-each-ref --format="%(align:30,left)refname is %(refname)%(end)|%(refname)" >actual &&
	test_cmp expect actual
'

test_expect_success 'middle alignment' '
	cat >expect <<-\EOF &&
	| refname is refs/heads/master |refs/heads/master
	|  refname is refs/heads/side  |refs/heads/side
	|   refname is refs/odd/spot   |refs/odd/spot
	|refname is refs/tags/double-tag|refs/tags/double-tag
	|  refname is refs/tags/four   |refs/tags/four
	|   refname is refs/tags/one   |refs/tags/one
	|refname is refs/tags/signed-tag|refs/tags/signed-tag
	|  refname is refs/tags/three  |refs/tags/three
	|   refname is refs/tags/two   |refs/tags/two
	EOF
	git for-each-ref --format="|%(align:30,middle)refname is %(refname)%(end)|%(refname)" >actual &&
	test_cmp expect actual
'

test_expect_success 'right alignment' '
	cat >expect <<-\EOF &&
	|  refname is refs/heads/master|refs/heads/master
	|    refname is refs/heads/side|refs/heads/side
	|      refname is refs/odd/spot|refs/odd/spot
	|refname is refs/tags/double-tag|refs/tags/double-tag
	|     refname is refs/tags/four|refs/tags/four
	|      refname is refs/tags/one|refs/tags/one
	|refname is refs/tags/signed-tag|refs/tags/signed-tag
	|    refname is refs/tags/three|refs/tags/three
	|      refname is refs/tags/two|refs/tags/two
	EOF
	git for-each-ref --format="|%(align:30,right)refname is %(refname)%(end)|%(refname)" >actual &&
	test_cmp expect actual
'

# Everything in between the %(align)...%(end) atom must be quoted, hence we test this by
# introducing single quote's in %(align)...%(end), which must not be escaped.

sq="'"

test_expect_success 'alignment with format quote' '
	cat >expect <<-EOF &&
	refname is ${sq}           ${sq}\\${sq}${sq}master${sq}\\${sq}${sq}           ${sq}|
	refname is ${sq}            ${sq}\\${sq}${sq}side${sq}\\${sq}${sq}            ${sq}|
	refname is ${sq}          ${sq}\\${sq}${sq}odd/spot${sq}\\${sq}${sq}          ${sq}|
	refname is ${sq}         ${sq}\\${sq}${sq}double-tag${sq}\\${sq}${sq}         ${sq}|
	refname is ${sq}            ${sq}\\${sq}${sq}four${sq}\\${sq}${sq}            ${sq}|
	refname is ${sq}            ${sq}\\${sq}${sq}one${sq}\\${sq}${sq}             ${sq}|
	refname is ${sq}         ${sq}\\${sq}${sq}signed-tag${sq}\\${sq}${sq}         ${sq}|
	refname is ${sq}           ${sq}\\${sq}${sq}three${sq}\\${sq}${sq}            ${sq}|
	refname is ${sq}            ${sq}\\${sq}${sq}two${sq}\\${sq}${sq}             ${sq}|
	EOF
	git for-each-ref --shell --format="refname is %(align:30,middle)${sq}%(refname:short)${sq}%(end)|" >actual &&
	test_cmp expect actual
'

test_expect_success 'setup for version sort' '
	test_commit foo1.3 &&
	test_commit foo1.6 &&
	test_commit foo1.10
'

test_expect_success 'version sort' '
	git for-each-ref --sort=version:refname --format="%(refname:short)" refs/tags/ | grep "foo" >actual &&
	cat >expect <<-\EOF &&
	foo1.3
	foo1.6
	foo1.10
	EOF
	test_cmp expect actual
'

test_expect_success 'version sort (shortened)' '
	git for-each-ref --sort=v:refname --format="%(refname:short)" refs/tags/ | grep "foo" >actual &&
	cat >expect <<-\EOF &&
	foo1.3
	foo1.6
	foo1.10
	EOF
	test_cmp expect actual
'

test_expect_success 'reverse version sort' '
	git for-each-ref --sort=-version:refname --format="%(refname:short)" refs/tags/ | grep "foo" >actual &&
	cat >expect <<-\EOF &&
	foo1.10
	foo1.6
	foo1.3
	EOF
	test_cmp expect actual
'

test_done
