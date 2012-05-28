
url="http://localhost/mediawiki/api.php"
test_rep="tXXXX_tmp_rep"

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
	my $username ='"'"'user'"'"';
	my $password = '"'"'password'"'"';
	my $mw = MediaWiki::API->new;
	$mw->{config}->{api_url} = $wikiurl;
	if (!defined($mw->login( { lgname => "$username",
		lgpassword => "$password" } ))) {
		die "getpage: login failed";
	}
	my $page = $mw->get_page( { title => $pagename } );
	if (!defined($page)) {
		die "getpage: wiki does not exist";
	}
	my $content = $page->{'"'"'*'"'"'};

	if (!defined($content)) {
		die "getpage: page does not exist";
	}
	$pagename=~s/\ /_/;
	open(file, ">$destdir/$pagename.mw");
	print file "$content";
	close (file);
	' "$1" "$2" $url
}


wiki_delete_page () {
#wiki_delete_page <page_name>
#delete the page <page_name> from the wiki.
	perl -e'
	use MediaWiki::API;

	my $wikiurl = $ARGV[1];
	my $pagename = $ARGV[0];
	my $login = '"'"'user'"'"';
	my $passwd= '"'"'password'"'"';


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
	my $text = "$wiki_content";
	if (defined($previous_text)) {
		$text="$previous_text\n\n$text";
	}
	$mw->edit( { action => '"'"'edit'"'"', title => "$wiki_page", text => "$text"} );' "$1" "$2" "$3" "$url"
}

git_content (){
#usage : git_content file_1 file_2
#precondition : file1 and file2 must exist
#behavior : if file_1 and file_2 do not match, the program exit with an error.

	result=$(diff -Bw $1 $2) || (echo "test failed: file $1 and $2 do not match" && exit 1)
}

wiki_page_content (){
#usage wiki_page_content <file> <page_name> 
#
#Exit with error code 1 if and only if the content of
#<page_name> and <file> do not match.

	wiki_getpage $2 .

	if [ -f $2.mw ]
	then
		git_content $1 $2.mw
	else
		echo "ERROR: file $2 not found on wiki"
		exit 1;
	fi
}

wiki_page_exist (){
#usage wiki_page_exist <page_name>
#Exit with error code 1 if and only if the page <page_name> is not on wiki

	wiki_getpage $1 .

	if [ ! -f $1.mw ] 
	then
		echo "ERROR : file $1 not found on wiki"
		exit 1;
	else 
		rm $1.mw
	fi
}

wiki_getallpagename () {
# wiki_getallpagename

#
# fetch all pages

	perl -e '
	use MediaWiki::API;
	my $wikiurl = $ARGV[0];
	my $username = user;
	my $password = password;
	my $mw = MediaWiki::API->new;
	$mw->{config}->{api_url} = $wikiurl;
	if (!defined($mw->login( { lgname => "$username",
		lgpassword => "$password" } ))) {
		die "getpage : login failed";
	}
	my $list = $mw->list ( { action => query,
                list => allpages,
                cmtitle => "Category:Surnames",
                cmnamespace => 0,
                cmlimit=> 500 },
                { max => 4, hook => \&cat_names } )
                || die $mw->{error}->{code}.": ".$mw->{error}->{details};
        # print the name of each article
        sub cat_names {
                my ($ref) = @_;
		
		open(file, ">all.txt");
                foreach (@$ref) {
		        print file "$_->{title}\n";	
                }
		close (file);
        }
	' "$url"
}

wiki_getallpage() {
	wiki_getallpagename
	mkdir $1
	while read -r line; do
		wiki_getpage "$line" $1;
	done < all.txt
	rm all.txt
}
