#!/bin/sh

test_description='Ignore binary file history'

. ./lib-git-p4.sh

test_expect_success 'start p4d' '
	start_p4d
'

test_expect_success 'Create repo with binary files' '
	client_view "//depot/... //client/..." &&
	(
		cd "$cli" &&

		echo "bin content rev1">file.dat &&
		p4 add -t binary file.dat &&
		echo "txt content rev1">file.txt &&
		p4 add file.txt &&
		p4 submit -d "rev1" &&

		p4 edit file.dat &&
		echo "bin content rev2">file.dat &&
		p4 submit -d "rev2"

		p4 edit file.dat &&
		echo "bin content rev3">file.dat &&
		p4 submit -d "rev3"
	)
'

test_expect_success 'Ignore binary content before CL 2' '
	client_view "//depot/... //client/..." &&
	test_when_finished cleanup_git &&
	(
		cd "$git" &&
		git init . &&
		git config git-p4.useClientSpec true &&
		git config git-p4.ignoreBinaryFileHistoryBefore 2 &&
		git p4 clone --destination="$git" //depot@all &&

		cat >expect <<-\EOF &&
			rev3
			[git-p4: depot-paths = "//depot/": change = 3]


			 file.dat | 2 +-
			 1 file changed, 1 insertion(+), 1 deletion(-)
			rev2
			[git-p4: depot-paths = "//depot/": change = 2]


			 file.dat | 1 +
			 1 file changed, 1 insertion(+)
			rev1
			[git-p4: depot-paths = "//depot/": change = 1]

			Ignored binaries on git-p4 import:
			add: //depot/file.dat#1


			 file.txt | 1 +
			 1 file changed, 1 insertion(+)
		EOF
		git log --format="%B" --stat >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'Ignore binary content before HEAD' '
	client_view "//depot/... //client/..." &&
	test_when_finished cleanup_git &&
	(
		cd "$git" &&
		git init . &&
		git config git-p4.useClientSpec true &&
		git config git-p4.ignoreBinaryFileHistoryBefore HEAD &&
		git p4 clone --destination="$git" //depot@all &&

		cat >expect <<-\EOF &&
			rev3
			[git-p4: depot-paths = "//depot/": change = 3]


			 file.dat | 1 +
			 1 file changed, 1 insertion(+)
			rev2
			[git-p4: depot-paths = "//depot/": change = 2]

			Ignored binaries on git-p4 import:
			edit: //depot/file.dat#2

			rev1
			[git-p4: depot-paths = "//depot/": change = 1]

			Ignored binaries on git-p4 import:
			add: //depot/file.dat#1


			 file.txt | 1 +
			 1 file changed, 1 insertion(+)
		EOF
		git log --format="%B" --stat >actual &&
		test_cmp expect actual
	)
'

test_expect_success 'kill p4d' '
	kill_p4d
'

test_done
