
test_rep="tXXXX_tmp_rep"

wiki_getpage () {
# wiki_getpage wiki_page dest_path
#
# fetch a page wiki_page from wiki and copies its content
# in directory dest_path
	../test-gitmw.pl "get_page" "$1" "$2"
}


wiki_delete_page () {
#wiki_delete_page <page_name>
#delete the page <page_name> from the wiki.
	../test-gitmw.pl "delete_page" "$1"
}

wiki_editpage (){
# wiki_editpage <wiki_page> <wiki_content> <wiki_append>
#
# Edit a page <wiki_page> on the wiki with content <wiki_content>
# If <wiki_append> == true : append
#
# If page doesn't exist : it creates page

	../test-gitmw.pl "edit_page" "$1" "$2" "$3"
}

git_content (){
#usage : git_content file_1 file_2
#precondition : file1 and file2 must exist
#behavior : if file_1 and file_2 do not match, the program exit with an error.

	result=$(diff -B -w $1 $2)

	if echo $result | grep -q ">" ; then
		echo "test failed: file $1 and $2 do not match"
		exit 1;
	fi
}

git_exist (){
#usage : git_exist rep_name file_name
#behavior : if file_name is not present in rep_name 
#or in his subdirectory, the program exit with an error
	result=$(find $1 -type f -name $2)

	if ! echo $result | grep -q $2; then
		echo "test failed: file $1/$2 does not exist"
		exit 1;
	fi

}

wiki_page_content (){
#usage wiki_page_content <file> <page_name> 
#
#Exit with error code 1 if and only if the content of
#<page_name> and <file> do not match.

	test -d $test_rep || mkdir $test_rep
	wiki_getpage $2 $test_rep

	if find $test_rep -name $2.mw -type f | grep -q $2; then
		git_content $1 $test_rep/$2.mw
		rm -rf $test_rep
	else
		rm -rf $test_rep
		echo "ERROR: file $2 not found on wiki"
		exit 1;
	fi
}

wiki_page_exist (){
#usage wiki_page_exist <page_name>
#Exit with error code 1 if and only if the page <page_name> is not on wiki


	test -d $test_rep || mkdir $test_rep
	wiki_getpage $1 $test_rep

	if find $test_rep -name $1.mw -type f | grep -q $1; then
		rm -rf $test_rep
	else
		rm -rf $test_rep
		echo "ERROR: file $1 not found on wiki"
		exit 1;
	fi
}

wiki_getallpagename () {
# wiki_getallpagename

# fetch all pages
	../test-gitmw.pl "getallpagename"
}

wiki_getallpage() {
	wiki_getallpagename
	mkdir $1
	while read -r line; do
		wiki_getpage "$line" $1;
	done < all.txt
	rm all.txt
}

