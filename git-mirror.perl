#!/usr/bin/env perl
# Copyright (C) 2009, Sam Vilain <sam@vilain.net>
# License: GPL v2 or later
use warnings;
use strict 'vars', 'subs';  # no strict refs ftw

use Scriptalicious;

{
	my $conf = {
	       };

	getopt();

	my $command = shift
		or abort "no command given";

	if (defined &{"do_$command"}) {
		&{"do_$command"}($conf, @ARGV);
	}
	else {
		abort "no such command '$command'";
	}
}
exit(0);

sub do_server {
	my $conf = shift;
	my $message = shift
		or abort "no message name given";

	if (defined &{'server_$message'}) {
		&{"server_$message"}($conf, @_);
	}
	else {
		abort "no such message '$message'";
	}
}

my @TODO = (
	do => "list",
	do => "notify",
	do => "sync",
	server => "list",
	server => "notify",
	server => "sync",
       );

while (my ($type, $message) = splice @TODO, 0, 2) {
	*{"${type}_$message"} = sub {
		my $conf = shift;
		my $name = $type eq "do" ? "command" : "message";
		abort "$name '$message' not yet implemented"
	};
}

#----------------------------------------
#  server_list
# Shows a list of current known mirrors of this repository.
#----------------------------------------
sub server_list {
	my $conf = shift;
	abort "no arguments to server_list expected; at '$_[0]'"
		if @_;

	
}


=head1 NAME

git-mirror - Git Mirroring administration commands

=head1 SYNOPSIS

 git mirror ( server )? { list | notify | sync } [ options ]

=head1 DESCRIPTION

B<git mirror> is responsible for retrieving and updating lists of
mirrors from repositories that have enabled public mirroring, as well
as swapping data with repositories which have enabled the I<mirror
sync> protocol.

B<git mirror list> would normally be called automatically by B<git
fetch> and the list of mirrors held for later fail-over.

B<git mirror server list> returns this node's idea about what mirrors
exist, possibly for a set of specified PGP keys only.

B<git mirror notify> would probably be invoked by something like C<git
clone --mirror --notify=URL>, to notify a repository of a new public
mirror, or a 'mob' branch.

B<git mirror server notify> receives such notice, perhaps checks that
the advertised URL is callable, decides on a TTL and records the notice.

B<git mirror sync> is a "peer to peer" protocol, so the "server"
keyword is redundant.  In this mode, the two nodes exchange
information about which 'push tags' are most current for the set of
PGP key IDs they are interested in - based on their configuration, and
then proceed to swap bundles which correspond to these objects.

=head1 COMMAND LINE OPTIONS

=over

=item B<-h, --help>

Display a program usage screen and exit.

=item B<-V, --version>

Display program version and exit.

=item B<-v, --verbose>

Verbose command execution, displaying things like the
commands run, their output, etc.

=item B<-q, --quiet>

Suppress all normal program output; only display errors and
warnings.

=item B<-d, --debug>

Display output to help someone debug this script, not the
process going on.

=back

=cut

# Various emacs/vim junk to try to make sure that the patches I get
# are already clean wrt whitespace :)

# Local Variables:
#   mode: cperl
#   cperl-brace-offset: 0
#   cperl-continued-brace-offset: 0
#   cperl-indent-level: 8
#   cperl-label-offset: -8
#   cperl-merge-trailing-else: nil
#   cperl-continued-statement-offset: 8
#   cperl-indent-parens-as-block: t
#   cperl-indent-wrt-brace: nil
# End:
#
# vim: vim:tw=78:sts=0:noet
