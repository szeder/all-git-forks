#!/bin/sh

test_description='Clone repositories with non ASCII paths'

. ./lib-git-p4.sh

UTF8_ESCAPED="a-\303\244_o-\303\266_u-\303\274.txt"

test_expect_success 'start p4d' '
	start_p4d
'

test_expect_success 'Create a repo containing iso8859-1 encoded paths' '
	cd "$cli" &&

	ISO8859="$(printf "$UTF8_ESCAPED" | iconv -f utf-8 -t iso8859-1)" &&
	>"$ISO8859" &&
	p4 add "$ISO8859" &&
	p4 submit -d "test commit"
'

test_expect_success 'Clone repo containing iso8859-1 encoded paths' '
	git p4 clone --destination="$git" --path-encoding=iso8859-1 //depot &&
	test_when_finished cleanup_git &&
	(
		cd "$git" &&
		printf "$UTF8_ESCAPED\n" >expect &&
		test_config core.quotepath false &&
		git ls-files >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'kill p4d' '
	kill_p4d
'

test_done
