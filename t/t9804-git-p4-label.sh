test_description='git-p4 p4 label tests'

. ./lib-git-p4.sh

test_expect_success 'start p4d' '
	start_p4d
'

# Basic p4 label tests.
#
# Note: can't have more than one label per commit - others
# are silently discarded.
#
test_expect_success 'basic p4 labels' '
	test_when_finished cleanup_git &&
	(
		cd "$cli" &&
		mkdir -p main &&

		echo f1 >main/f1 &&
		p4 add main/f1 &&
		p4 submit -d "main/f1" &&

		echo f2 >main/f2 &&
		p4 add main/f2 &&
		p4 submit -d "main/f2" &&

		echo f3 >main/file_with_\$metachar &&
		p4 add main/file_with_\$metachar &&
		p4 submit -d "file with metachar" &&

		p4 tag -l tag_f1_only main/f1 &&
		p4 tag -l tag_with\$_shell_char main/... &&

		echo f4 >main/f4 &&
		p4 add main/f4 &&
		p4 submit -d "main/f4" &&

		p4 label -i <<-EOF &&
		Label: long_label
		Description:
		   A Label first line
		   A Label second line
		View:	//depot/...
		EOF

		p4 tag -l long_label ... &&

		p4 labels ... &&

		cd "$git" &&
		pwd &&
		"$GITP4" clone --dest=. --detect-labels //depot@all &&

		git tag &&
		git tag >taglist &&
		test_line_count = 3 taglist &&

		cd main &&
		git checkout tag_tag_f1_only &&
		! test -f f2 &&
		git checkout tag_tag_with\$_shell_char &&
		test -f f1 && test -f f2 && test -f file_with_\$metachar &&

		git show tag_long_label | grep -q "A Label second line"
	)
'

# Test some label corner cases:
#
# - two tags on the same file; both should be available
# - a tag that is only on one file; the other file should
#   not be checked out on the tag.

test_expect_failure 'two labels on the same changelist' '
	test_when_finished cleanup_git &&
	(
		cd "$cli" &&
		mkdir -p main &&

		p4 edit main/f1 main/f2 &&
		echo "hello world" >main/f1 &&
		echo "not in the tag" >main/f2 &&
		p4 submit -d "main/f[12]: testing two labels" &&

		p4 tag -l tag_f1_1 main/f1 &&
		p4 tag -l tag_f1_2 main/f1 &&

		p4 labels ... &&

		cd "$git" &&
		pwd &&
		"$GITP4" clone --dest=. --detect-labels //depot@all &&

		git tag | grep -q tag_f1_1 &&
		git tag | grep -q tag_f1_2 &&

		cd main &&

		git checkout tag_tag_f1_1 &&
		test -f f1 &&

		git checkout tag_tag_f1_1 &&
		test -f f1 &&

		! test -f f2
	)
'

test_expect_success 'kill p4d' '
	kill_p4d
'

test_done
