
url="http://localhost/mediawiki/api.php"

wiki_getpage () {
# wiki_getpage wiki_page dest_path
#
# fetch a page wiki_page from wiki and copies its content
# in directory dest_path
	perl -e '
	use MediaWiki::API;

	my $pagename = $ARGV[0];
	my $wikiurl = $ARGV[2];
	my $destdir = $ARGV[1];
	my $username = "user";
	my $password = "password";
	my $mw = MediaWiki::API->new;
	$mw->{config}->{api_url} = $wikiurl;
	if (!defined($mw->login( { lgname => "$username",
		lgpassword => "$password" } ))) {
		die "getpage : login failed";
	}
	my $page = $mw->get_page( { title => $pagename } );
	if (!defined($page)) {
		die "getpage : wiki does not exist";
	}
	my $content = $page->{'"'"'*'"'"'};

	if (!defined($content)) {
		die "getpage : page does not exist";
	}
	open(file, ">$destdir/$pagename");
	print file "$content";
	close (file);
	' $1 $2 $url
}


wiki_delete_page () {
#wiki_delete_page <page_name>
#delete the page <page_name> from the wiki.
	perl -e'
	use MediaWiki::API;

	my $wikiurl = $ARGV[1];
	my $pagename = $ARGV[0];
	my $login = user;
	my $passwd= password;


	my $mw = MediaWiki::API->new({api_url => $wikiurl});
	$mw->login({lgname => $login, lgpassword => $passwd });

	my $exist=$mw->get_page({title => $pagename});

	if (defined($exist->{'"'"'*'"'"'})){
		$mw->edit({
		action => '"'"'delete'"'"',
		title => $pagename})
		|| die $mw->{error}->{code} . ": " . $mw->{error}->{details};
	}else{
	die "no page with such name found : $pagename\n";
	}' $1 $url
}

wiki_editpage (){
# wiki_editpage <wiki_page> <wiki_content> <wiki_append>
#
# Edit a page <wiki_page> on the wiki with content <wiki_content>
# If <wiki_append> == true : append
#
# If page doesn't exist : it creates page

	perl -e '
	use MediaWiki::API;

	my $wiki_page = $ARGV[0];
	my $wiki_content = $ARGV[1];
	my $wiki_append = $ARGV[2];
	my $wiki_url = $ARGV[3];
	my $wiki_login = user;
	my $wiki_password = password;

	my $append = 0;
	if (defined($wiki_append) && $wiki_append eq '"'"'true'"'"') {
		$append=1;
	}


	my $mw = MediaWiki::API->new;
	$mw->{config}->{api_url} = $wiki_url;
	$mw->login({lgname => $wiki_login, lgpassword => $wiki_pass });
	my $previous_text ="";

	if ($append) {
		my $ref = $mw->get_page( { title => $wiki_page } );
		$previous_text = $ref->{'"'"'*'"'"'};
	}
	my $text = $wiki_content;
	if (defined($previous_text)) {
		$text="$previous_text\n\n$text";
	}
	$mw->edit( { action => '"'"'edit'"'"', title => $wiki_page, text => "$text"} );' "$1" "$2" "$3" "$url"
}

git_content (){
#usage : git_content.sh file_1 file_2
#precondition : file1 and file2 must exist
#behavior : if file_1 and file_2 do not match, the program exit with an error.

	result=$(diff $1 $2)

	if echo $result | grep -q ">" ; then
		echo "test failed : file $1 and $2 do not match"
		exit 1;
	fi
}

git_exist (){
#usage : git_exist rep_name file_name
#behavior : if file_name is not present in rep_name 
#or in his subdirectory, the program exit with an error
	result=$(find $1 -type f -name $2)

	if ! echo $result | grep -q $2; then
		echo "test failed : file $1/$2 does not exist"
		exit 1;
	fi

}

wiki_page_content (){
#usage wiki_page_content <file> <page_name> 
#
#Exit with error code 1 if and only if the content of
#<page_name> and <file> do not match.

	test -d ./tmp_test || mkdir ./tmp_test
	wiki_getpage $2 ./tmp_test

	if find ./tmp_test -name $2 -type f | grep -q $2; then
		git_content.sh $1 ./tmp_test/$2
		rm -rf ./tmp_test
	else
		rm -rf ./tmp_test
		echo "ERROR : file $2 not found on wiki"
		exit 1;
	fi
}

wiki_page_exist (){
#usage wiki_page_exist <page_name>
#Exit with error code 1 if and only if the page <page_name> is not on wiki


	test -d ./tmp_test || mkdir ./tmp_test
	wiki_getpage $1 ./tmp_test

	if find ./tmp_test/ -name $1 -type f | grep -q $1; then
		rm -rf tmp_test
	else
		rm -rf ./tmp_test
		echo "ERROR : file $1 not found on wiki"
		exit 1;
	fi
}


