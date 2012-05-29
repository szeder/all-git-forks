#!/usr/bin/perl -w
# Copyright (C) 2012
#     Charles Roussel <charles.roussel@ensimag.imag.fr>
#     Simon Cathebras <simon.cathebras@ensimag.imag.fr>
#     Julien Khayat <julien.khayat@ensimag.imag.fr>
#     Guillaume Sasdy <guillaume.sasdy@ensimag.imag.fr>
#     Simon Perrat <simon.perrat@ensimag.imag.fr>
# License:
# Usage: execute this program in terminal, using in first argument
# the name of the function to call. The other arguments are coherent to 
# the function called.
#	./test-gitmw.pl TO_CALL [argument]*
# For example:
#	<./test-gitmw.pl "get_page" foo .>
#	will call the function "wiki_getpage" with the arguments <foo> and <.>
#
# Possible value for TO_CALL:
#	"get_page"
#	"delete_page"
#	"edit_page"
#	"getallpagename"
#
#

use MediaWiki::API;
use Switch;
#URL of the wiki used for the tests
my $wiki_url="http://localhost/wiki/api.php";
my $wiki_admin='WikiAdmin';
my $wiki_admin_pass='AdminPass';
my $mw = MediaWiki::API->new;
$mw->{config}->{api_url} = $wiki_url;

sub wiki_login {
	$mw->login( { lgname => "$_[0]",lgpassword => "$_[1]" } ) || die "getpage: login failed";
}

sub wiki_getpage {
# wiki_getpage(wiki_page,dest_path)
	#
# fetch a page wiki_page from wiki and copies its content
# in directory dest_path
	my $pagename = $_[0];
	my $destdir = $_[1];
	
	my $page = $mw->get_page( { title => $pagename } );
	if (!defined($page)) {
		die "getpage: wiki does not exist";
	}
	my $content = $page->{'*'};

	if (!defined($content)) {
		die "getpage: page does not exist";
	}
	$pagename=~s/\ /_/;
	open(my $file, ">$destdir/$pagename.mw");
	print $file "$content";
	close ($file);
}


sub wiki_delete_page {
#wiki_delete_page(page_name)
#delete the page <page_name> from the wiki.
	my $pagename = $_[0];

	my $exist=$mw->get_page({title => $pagename});

	if (defined($exist->{'*'})){
		$mw->edit({
				action => 'delete',
				title => $pagename})
		|| die $mw->{error}->{code} . ": " . $mw->{error}->{details};
	}else{
		die "no page with such name found: $pagename\n";
	}
}

sub wiki_editpage {
# wiki_editpage (wiki_page, wiki_content, wiki_append)
	#
# Edit a page <wiki_page> on the wiki with content <wiki_content>
# If <wiki_append> == true : append
	#
# If page doesn't exist : it creates page
	my $wiki_page = $_[0];
	my $wiki_content = $_[1];
	my $wiki_append = $_[2];

	my $append = 0;
	if (defined($wiki_append) && $wiki_append eq 'true') {
		$append=1;
	}

	my $previous_text ="";

	if ($append) {
	my $ref = $mw->get_page( { title => $wiki_page } );
		$previous_text = $ref->{'*'};
	}
	my $text = $wiki_content;
	if (defined($previous_text)) {
		$text="$previous_text\n\n$text";
	}
	$mw->edit( { action => 'edit', title => $wiki_page, text => "$text"} );
}

sub wiki_getallpagename {
# wiki_getallpagename()

# fetch all pages
	$mw->list ( { action => 'query',
			list => 'allpages',
			#cmtitle => "Category:Surnames",
			#cmnamespace => 0,
			cmlimit=> 500 },
		{ max => 4, hook => \&cat_names } )
	|| die $mw->{error}->{code}.": ".$mw->{error}->{details};
	# print the name of each article
	sub cat_names {
		my ($ref) = @_;

		open(my $file, ">all.txt");
		foreach (@$ref) {
			print $file "$_->{title}\n";
		}
		close ($file);
	}
}

#Selecting the function to use

my $to_call = shift;

my $login = $ARGV[0];
if ($login eq "-p")
{
	&wiki_login($wiki_admin,$wiki_admin_pass);
	shift;
}

switch ($to_call) {
	case "get_page" { &wiki_getpage($ARGV[0], $ARGV[1])}
	case "delete_page" { &wiki_delete_page($ARGV[0])}
	case "edit_page" { &wiki_editpage($ARGV[0], $ARGV[1], $ARGV[2])}
	case "getallpagename" { &wiki_getallpagename()}
	else{ die("test-gitmw.pl ERROR: unknown input")}
}
