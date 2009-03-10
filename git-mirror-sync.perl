#!/usr/bin/perl -w

# git-mirror-sync.perl
#
# script to handle the data exchange part of the mirror-sync protocol.
#
# this program is essentially a peer to peer node.  its standard input
# is connected to one node, and its standard output another.  state
# and communication with other git-mirror-sync processes must be
# carried out through state files in the local repository (or IPC I
# guess, but state will be simpler)

use strict;
use Git;
our $git = Git->repository;

#  we need to know to begin;
#
#    - what PGP key(s) we are considering authorative/interesting for
#      download
#    - what local ref namespace the finished refs will end up at
#
#  This information I expect to be passed in by the function in
#  daemon.c which checks the configuration for the repository.  It
#  could also conceivably be read by this script.

use Getopt::Long qw(bundling);

our @auth_keys;
our $refs = "refs/";

GetOptions(
	"auth=s@" => \@auth_keys,
	"refs=s" => \$refs,
	);

# this iterator returns a line or puts a line back, depending on how
# it is called.  don't call it if you don't expect any data; later
# this can be replaced by an event loop.
our $readline = do {
	my @buffer;
	sub {
		if ( @_ ) {
			unshift @buffer, @_;
		}
		elsif ( @buffer ) {
			return shift @buffer;
		}
		else {
			my $line = <STDIN>;
			if ( defined $line ) {
				chomp($line);
			}
			$line;
		}
	};
};

alarm 60;

#  the first phase is to figure out what "push" objects we have
#  locally, and swap them with the other side.  a "push" object is
#  represented as a tag which contains in the tag description the
#  state of all of the refs in the repository at the time; ie, it's
#  like a packed-refs file.

# references are stored under refs/pushes/KEYID/X (the X is a local
# number and unimportant)
my @keys = map { [ m{pushes/([^/]+)/(\d+\w*) ([a-f0-9]+)$} ] }
	$git->command(
		"for-each-ref", "--format=%(refname) %(objectname)",
		"refs/pushes/*",
		);

# find out about and tell the other end about the latest ones we have
my %latest;
for ( @keys ) {
	my ($keyid, $num, $ref) = @$keys;
	if ( not exists $latest{$keyid} or $latest{$keyid}[0] < $num ) {
		$latest{$keyid} = [ $num, $ref ];
	}
}

for my $latest ( keys %latest ) {
	my ($num, $ref) = @$latest;
	print "have push ".$latest->[1]." auth $latest\n";
}

print "got push?\n";
my $line;

# find out about their latest pushes
my %theirs;
while ( defined( $line = $readline->() ) ) {
	if ( $line eq "got push?" ) {
		last;
	}
	elsif ( $line =~ /^have push ([0-9a-f]+) auth ([0-9a-f]+)$/ ) {
		$theirs{$2} = $1;
	}
	else {
		bomb_out("push");
	}
}

# okay.  so, first of all - are we interested in any pushes they have?
my %requested;
for my $auth_key ( @auth_keys ) {
	if ( my $theirs = $theirs{$auth_key} ) {

		my $found = grep {
			$theirs eq substr($_->[2], 0, length($theirs))
		} @keys;

		if ( not $found ) {
			print "fetch push $theirs\n";
			$requested{$theirs} = $auth_key;
		}
	}
}

print "want push?\n";

# listen for push requests
while ( defined( my $line = $readline->() ) ) {
	if ( $line eq "want push?" ) {
		last;
	}
	elsif ( $line =~ /^receive push ([0-9a-f]+) size (\d+)$/ ) {
		bomb_out("push_requests")
			unless $requested{$1};

		my ($objectname, $auth_key, $size) =
			($1, $requested{$1}, $2);

		my $tag_data = do {
			local($/) = \$size;
			<STDIN>;
		};

		bomb_out("receive_push", "size")
			if length $tag_data ne $size;

		my ($pid, $pin, $pout, $ctx) = $git->command_bidi_pipe
			("hash-object", "-t", "tag", "--stdin");
		print { $pin } $tag_data;
		close $pin;
		my $hash = <$pout>;
		$git->command_close_bidi_pipe($pid, $pin, $pout, $ctx);

		chomp($hash);
		if ( substr($hash, 0, length($objectname) ) ne $objectname ) {
			bomb_out("receive_push", "hash");
		}
		else {
			# write it out

			# first, find out whether it's newer than the
			# known pushes or older.

			my ($tagged) =
				$tag_data =~ m{^object ([a-f0-9]+)$}m;

			my $tag_date =
				$tag_data =~ m{^tagger .* (\d+)\s*([+\-]\d+)$}m;

			my $target_ref;
			my $num;

			if ( my $key = grep { $_->[2] eq $tagged } @keys ) {
				# ideal situation - exactly placed
				if ( grep { $_->[0] eq $auth_key and
						    $_->[1] > $key->[1] }
					     @keys ) {
					# fits in between a previous key
					$num = $key->[1];
					my $suffix = "a";
					while ( grep { $_->[1] eq $num.$suffix }
						@keys ) {
						$suffix++;
					}
					$num .= $suffix;
				}
				else {
					# it's a new key!
					$num = $key->[1]+1;
					$target_ref = "refs/pushes/"
						."$auth_key/$num";
				}
			}
			else {
				# suboptimal - can't find the ref it
				# builds from; use dates
				&{"..."}();
			}

			# now we've got the ref name, write it out.
			($pid, $pin, $pout, $ctx) = $git->command_bidi_pipe
				("hash-object", "-w", "-t", "tag", "--stdin");
			print { $pin } $tag_data;
			close $pin;
			my $new_hash = <$pout>;
			$hash eq $new_hash
				or die "insane output from hash-object";
			$git->command_close_bidi_pipe($pid, $pin, $pout, $ctx);

			# FIXME - write to a temp reference, and
			# verify the tag.
			$git->command_oneline(
				"update-ref", $target_ref, $hash,
				);
			push @keys, [ $auth_key, $num, $hash ];
			$latest{$auth_key} = [ $num, $hash ];
		}
	}
	elsif ( $line =~ /^fetch push ([0-9a-f]+)$/ ) {
		my ($push) = grep {
			$theirs eq substr($_->[2], 0, length($theirs))
		} @keys;
		if ( $push ) {
			my $push_data = $git->command(
				"cat-file", "tag", $push
				);
			print "receive push $push size "
				.length($push_data)."\n";
			print $push_data;
		}
	}
}

# having now swapped push objects, we figure out what we're trying to
# achieve.  Easy - just the latest verified pushes for the PGP keys
# we're tracking.
my @targets;
for my $key ( @auth_keys ) {
	push @targets, $latest{$key};
}

# next, figure out what we've /got/

# if we've got bundle fragments or prepared bundles already, then add
# those to our inventory, and base our bundles on them.
our %bundles = scan_bundles(@targets);

# spew said inventory to the other end
while ( my ($bundle_id, $info) = each %bundles ) {
	my ($approx_size, $bitmap) = @$info;
	if ( $bitmap =~ /^1+$/ ) {
		$bitmap = "all";
	}
	else {
		$bitmap = " bitmap $bitmap";
	}
	print "have bundle $bundle_id size $approx_size $bitmap\n";
}

print "want bundle?\n";

# collect what they got
my %their_bundles;
while ( defined( my $line = $readline->() ) ) {
	if ( $line eq "want bundle?" ) {
		last;
	}
	elsif ( $line =~ m{have \s bundle \s ([a-f0-9]+:[0-9a-f]+)
			   \s+ size (\d+ [kmgtp]?)
			   \s+ ( all | [01]+ )
			   $}x) {
		$their_bundles{$1} = [ $2, $3 ]
	}
	else {
		bomb_out("receive_bundle", "bad input")
	}
}

# ok.  Now, we enter a loop, that is terminated when we have solved
# both the local insufficiency and the remote insufficiency.
my $local_ok = bitmaps_full(\%bundles);
my $remote_ok = bitmaps_full(\%their_bundles);

while ( !$local_ok and !$remote_ok ) {

	# look for a block that they have, that we want, and request it.

	my @we_want;

	# as we can't interleave data messages and requests, and we
	# want to keep both channels active, at some point we might
	# want to try to pipeline the fetch requests proportionately
	# to the received fetch requests.  Until then we probably just
	# want to solve it one by one.
	if ( $local_ok ) {
		print "I'm good.\n";
	}
	else {

		# ...look for a bundle that they have, which would be
		# useful to us...

		print "want bundle X:Y\n";
		push @we_want, [ "X:Y" => undef ];

		# for fractional bundle transfer (download spreading),
		# we will want to ask for a particular fragment of the
		# bundle.  This could be specified like this:
		# print "want bundle X:Y fragment 3/13\n";
		# push @we_want, [ "X:Y" => "0010000000000" ];
	}

	my @they_want;
	unless ( $remote_ok ) {
		my $they_want = $readline->();
		if ( $they_want =~ /I'm good/ ) {
			$remote_ok = 1;
		}
		elsif ( $they_want =~ m{want \s+ bundle \s+
					([a-f0-9]+):([0-9a-f]+)
					(?: \s+ fragment \s+ (\d+)/(\d+) )?
			       }x) {
			my $bitmap_wanted = $4 ? "0" x $4 : undef;
			if ( $4 ) {
				substr($bitmap_wanted, $3-1, 1) = "1";
			}
			push @they_want, [ "$1:$2", $bitmap_wanted ];
		}
		else {
			bomb_out("swap_data", "expecting want");
		}
	}

	my $sent_bytes;
	if ( @they_want ) {
		# ... open a filehandle to a bundle ...
		# open SENDING, "git bundle ...|";
		# open SENDING, "<".$git->repo_path."/objects/frags/XXX.frag";
		print "receive bundle @{$they_want[0]} size XXX\n";  # not quite right
	}
	if ( @we_want ) {
		my $they_give = $readline->();
		if ( $they_give =~ m{receive bundle ... size (\d+)} ) {
			# ... note that we have to receive X bytes...
		}
	}

	# exchange data - need to use select here to avoid deadlock.
	while ( select(...) ) {
		# ... write data, read data, etc.
	}

	$local_ok = bitmaps_full(\%bundles);
	$remote_ok ||= bitmaps_full(\%their_bundles);
}

sub scan_bundles {
	my @targets = @_;

	# this function needs to determine which bundles we have,
	# based on state information for incomplete bundles and
	# verifying refs for complete bundles.
	#
	# It returns a list;
	# "$src:$dst" => [ $approx_size, $bitmap ];

	# '$src' and '$dst' are the object IDs of the push objects
	# which define their start and end.  if it is the beginning,
	# probably all 0's or the empty string is right.  The $bitmap
	# field is for the fractional bundle support.
}

sub bitmaps_full {
	my $pack_list = shift;
	while ( my ($pack, $info) = each %$pack_list ) {
		if ( $info->[2] =~ m{0} ) {
			return undef;
		}
	}
	return 1;
}

sub bomb_out {
	my $stage = shift;
	my $error = shift;
	print "E:conversation error in '$stage' stage\n";
	die "died during '$stage' stage"
		.($error ? " (error: $error)" : "" );
}

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
