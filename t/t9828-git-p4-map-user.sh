#!/bin/sh

test_description='Clone repositories and map users'

. ./lib-git-p4.sh

test_expect_success 'start p4d' '
	start_p4d
'

test_expect_success 'Create a repo with different users' '
	client_view "//depot/... //client/..." &&
	(
		cd "$cli" &&

		>author.txt &&
		p4 add author.txt &&
		p4 submit -d "Add file author\\n"

		P4USER=mmax
		>max.txt &&
		p4 add max.txt &&
		p4 submit -d "Add file max"

		P4USER=mo
		>moritz.txt &&
		p4 add moritz.txt &&
		p4 submit -d "Add file moritz"

		P4USER=no
		>nobody.txt &&
		p4 add nobody.txt &&
		p4 submit -d "Add file nobody"
	)
'

test_expect_success 'Clone repo root path with all history' '
	client_view "//depot/... //client/..." &&
	test_when_finished cleanup_git &&
	(
		cd "$git" &&
		git init . &&
		git config --add git-p4.mapUser "mmax -> Max Mustermann <max@muster.com>"  &&
		git config --add git-p4.mapUser "mo -> Moritz Untreu <moritz@untreu.com>" &&
		git p4 clone --use-client-spec --destination="$git" //depot@all &&
		cat >expect <<-\EOF &&
			no <no@client>
			Moritz Untreu <moritz@untreu.com>
			Max Mustermann <max@muster.com>
			Dr. author <author@example.com>
		EOF
		git log --format="%an <%ae>" >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'kill p4d' '
	kill_p4d
'

test_done
