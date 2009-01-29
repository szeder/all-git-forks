#!/usr/bin/perl -w
#
# Copyright 2002,2005 Greg Kroah-Hartman <greg@kroah.com>
# Copyright 2005 Ryan Anderson <ryan@michonline.com>
#
# GPL v2 (See COPYING)
#
# Ported to support git "mbox" format files by Ryan Anderson <ryan@michonline.com>
#
# Sends a collection of emails to the given email addresses, disturbingly fast.
#
# Supports two formats:
# 1. mbox format files (ignoring most headers and MIME formatting - this is designed for sending patches)
# 2. The original format support by Greg's script:
#    first line of the message is who to CC,
#    and second line is the subject of the message.
#

use strict;
use warnings;
use Term::ReadLine;
use Getopt::Long;
use Data::Dumper;
use Term::ANSIColor;
use Git;

$SIG{INT} = sub { print color("reset"), "\n"; exit };

package FakeTerm;
sub new {
	my ($class, $reason) = @_;
	return bless \$reason, shift;
}
sub readline {
	my $self = shift;
	die "Cannot use readline on FakeTerm: $$self";
}
package main;


sub usage {
	print <<EOT;
git-send-email [options] <file | directory>...
Options:
   --from         Specify the "From:" line of the email to be sent.

   --to           Specify the primary "To:" line of the email.

   --cc           Specify an initial "Cc:" list for the entire series
                  of emails.

   --cc-cmd       Specify a command to execute per file which adds
                  per file specific cc address entries

   --bcc          Specify a list of email addresses that should be Bcc:
		  on all the emails.

   --compose      Use \$GIT_EDITOR, core.editor, \$EDITOR, or \$VISUAL to edit
		  an introductory message for the patch series.

   --subject      Specify the initial "Subject:" line.
                  Only necessary if --compose is also set.  If --compose
		  is not set, this will be prompted for.

   --in-reply-to  Specify the first "In-Reply-To:" header line.
                  Only used if --compose is also set.  If --compose is not
		  set, this will be prompted for.

   --chain-reply-to If set, the replies will all be to the previous
                  email sent, rather than to the first email sent.
                  Defaults to on.

   --signed-off-cc Automatically add email addresses that appear in
                 Signed-off-by: or Cc: lines to the cc: list. Defaults to on.

   --smtp-server  If set, specifies the outgoing SMTP server to use.
                  Defaults to localhost.

   --suppress-from Suppress sending emails to yourself if your address
                  appears in a From: line. Defaults to off.

   --thread       Specify that the "In-Reply-To:" header should be set on all
                  emails. Defaults to on.

   --quiet	  Make git-send-email less verbose.  One line per email
                  should be all that is output.

   --dry-run	  Do everything except actually send the emails.

   --envelope-sender	Specify the envelope sender used to send the emails.

EOT
	exit(1);
}

# most mail servers generate the Date: header, but not all...
sub format_2822_time {
	my ($time) = @_;
	my @localtm = localtime($time);
	my @gmttm = gmtime($time);
	my $localmin = $localtm[1] + $localtm[2] * 60;
	my $gmtmin = $gmttm[1] + $gmttm[2] * 60;
	if ($localtm[0] != $gmttm[0]) {
		die "local zone differs from GMT by a non-minute interval\n";
	}
	if ((($gmttm[6] + 1) % 7) == $localtm[6]) {
		$localmin += 1440;
	} elsif ((($gmttm[6] - 1) % 7) == $localtm[6]) {
		$localmin -= 1440;
	} elsif ($gmttm[6] != $localtm[6]) {
		die "local time offset greater than or equal to 24 hours\n";
	}
	my $offset = $localmin - $gmtmin;
	my $offhour = $offset / 60;
	my $offmin = abs($offset % 60);
	if (abs($offhour) >= 24) {
		die ("local time offset greater than or equal to 24 hours\n");
	}

	return sprintf("%s, %2d %s %d %02d:%02d:%02d %s%02d%02d",
		       qw(Sun Mon Tue Wed Thu Fri Sat)[$localtm[6]],
		       $localtm[3],
		       qw(Jan Feb Mar Apr May Jun
			  Jul Aug Sep Oct Nov Dec)[$localtm[4]],
		       $localtm[5]+1900,
		       $localtm[2],
		       $localtm[1],
		       $localtm[0],
		       ($offset >= 0) ? '+' : '-',
		       abs($offhour),
		       $offmin,
		       );
}

my $have_email_valid = eval { require Email::Valid; 1 };
my $smtp;

sub unique_email_list(@);
sub cleanup_compose_files();

# Constants (essentially)
my $compose_filename = ".msg.$$";

# Variables we fill in automatically, or via prompting:
my (@to,@cc,@initial_cc,@bcclist,@xh,
	$initial_reply_to,$initial_subject,@files,$author,$sender,$compose,$time);

my $smtp_server;
my $envelope_sender;

# Example reply to:
#$initial_reply_to = ''; #<20050203173208.GA23964@foobar.com>';

my $repo = Git->repository();
my $term = eval {
	new Term::ReadLine 'git-send-email';
};
if ($@) {
	$term = new FakeTerm "$@: going non-interactive";
}

# Behavior modification variables
my ($quiet, $dry_run) = (0, 0);

# Variables with corresponding config settings
my ($thread, $chain_reply_to, $suppress_from, $signed_off_cc, $cc_cmd);

my %config_settings = (
    "thread" => [\$thread, 1],
    "chainreplyto" => [\$chain_reply_to, 1],
    "suppressfrom" => [\$suppress_from, 0],
    "signedoffcc" => [\$signed_off_cc, 1],
    "cccmd" => [\$cc_cmd, ""],
);

foreach my $setting (keys %config_settings) {
    my $config = $repo->config_bool("sendemail.$setting");
    ${$config_settings{$setting}->[0]} = (defined $config) ? $config : $config_settings{$setting}->[1];
}

@bcclist = $repo->config('sendemail.bcc');
if (!@bcclist or !$bcclist[0]) {
    @bcclist = ();
}

# Begin by accumulating all the variables (defined above), that we will end up
# needing, first, from the command line:

my $rc = GetOptions("sender|from=s" => \$sender,
                    "in-reply-to=s" => \$initial_reply_to,
		    "subject=s" => \$initial_subject,
		    "to=s" => \@to,
		    "cc=s" => \@initial_cc,
		    "bcc=s" => \@bcclist,
		    "chain-reply-to!" => \$chain_reply_to,
		    "smtp-server=s" => \$smtp_server,
		    "compose" => \$compose,
		    "quiet" => \$quiet,
		    "cc-cmd=s" => \$cc_cmd,
		    "suppress-from!" => \$suppress_from,
		    "signed-off-cc|signed-off-by-cc!" => \$signed_off_cc,
		    "dry-run" => \$dry_run,
		    "envelope-sender=s" => \$envelope_sender,
		    "thread!" => \$thread,
	 );

unless ($rc) {
    usage();
}

# Verify the user input

foreach my $entry (@to) {
	die "Comma in --to entry: $entry'\n" unless $entry !~ m/,/;
}

foreach my $entry (@initial_cc) {
	die "Comma in --cc entry: $entry'\n" unless $entry !~ m/,/;
}

foreach my $entry (@bcclist) {
	die "Comma in --bcclist entry: $entry'\n" unless $entry !~ m/,/;
}

# Now, let's fill any that aren't set in with defaults:

my ($repoauthor) = $repo->ident_person('author');
my ($repocommitter) = $repo->ident_person('committer');

my %aliases;
my @alias_files = $repo->config('sendemail.aliasesfile');
my $aliasfiletype = $repo->config('sendemail.aliasfiletype');
my %parse_alias = (
	# multiline formats can be supported in the future
	mutt => sub { my $fh = shift; while (<$fh>) {
		if (/^\s*alias\s+(\S+)\s+(.*)$/) {
			my ($alias, $addr) = ($1, $2);
			$addr =~ s/#.*$//; # mutt allows # comments
			 # commas delimit multiple addresses
			$aliases{$alias} = [ split(/\s*,\s*/, $addr) ];
		}}},
	mailrc => sub { my $fh = shift; while (<$fh>) {
		if (/^alias\s+(\S+)\s+(.*)$/) {
			# spaces delimit multiple addresses
			$aliases{$1} = [ split(/\s+/, $2) ];
		}}},
	pine => sub { my $fh = shift; while (<$fh>) {
		if (/^(\S+)\t.*\t(.*)$/) {
			$aliases{$1} = [ split(/\s*,\s*/, $2) ];
		}}},
	gnus => sub { my $fh = shift; while (<$fh>) {
		if (/\(define-mail-alias\s+"(\S+?)"\s+"(\S+?)"\)/) {
			$aliases{$1} = [ $2 ];
		}}}
);

if (@alias_files and $aliasfiletype and defined $parse_alias{$aliasfiletype}) {
	foreach my $file (@alias_files) {
		open my $fh, '<', $file or die "opening $file: $!\n";
		$parse_alias{$aliasfiletype}->($fh);
		close $fh;
	}
}

($sender) = expand_aliases($sender) if defined $sender;

my $prompting = 0;
if (!defined $sender) {
	$sender = $repoauthor || $repocommitter;
	do {
		$_ = $term->readline("Who should the emails appear to be from? [$sender] ");
	} while (!defined $_);

	$sender = $_ if ($_);
	print "Emails will be sent from: ", $sender, "\n";
	$prompting++;
}

if (!@to) {
	do {
		$_ = $term->readline("Who should the emails be sent to? ",
				"");
	} while (!defined $_);
	my $to = $_;
	push @to, split /,/, $to;
	$prompting++;
}

sub expand_aliases {
	my @cur = @_;
	my @last;
	do {
		@last = @cur;
		@cur = map { $aliases{$_} ? @{$aliases{$_}} : $_ } @last;
	} while (join(',',@cur) ne join(',',@last));
	return @cur;
}

@to = expand_aliases(@to);
@to = (map { sanitize_address($_) } @to);
@initial_cc = expand_aliases(@initial_cc);
@bcclist = expand_aliases(@bcclist);

if (!defined $initial_subject && $compose) {
	do {
		$_ = $term->readline("What subject should the initial email start with? ",
			$initial_subject);
	} while (!defined $_);
	$initial_subject = $_;
	$prompting++;
}

if ($thread && !defined $initial_reply_to && $prompting) {
	do {
		$_= $term->readline("Message-ID to be used as In-Reply-To for the first email? ",
			$initial_reply_to);
	} while (!defined $_);

	$initial_reply_to = $_;
	$initial_reply_to =~ s/^\s+<?/</;
	$initial_reply_to =~ s/>?\s+$/>/;
}

if (!$smtp_server) {
	$smtp_server = $repo->config('sendemail.smtpserver');
}
if (!$smtp_server) {
	foreach (qw( /usr/sbin/sendmail /usr/lib/sendmail )) {
		if (-x $_) {
			$smtp_server = $_;
			last;
		}
	}
	$smtp_server ||= 'localhost'; # could be 127.0.0.1, too... *shrug*
}

if ($compose) {
	# Note that this does not need to be secure, but we will make a small
	# effort to have it be unique
	open(C,">",$compose_filename)
		or die "Failed to open for writing $compose_filename: $!";
	print C "From $sender # This line is ignored.\n";
	printf C "Subject: %s\n\n", $initial_subject;
	printf C <<EOT;
GIT: Please enter your email below.
GIT: Lines beginning in "GIT: " will be removed.
GIT: Consider including an overall diffstat or table of contents
GIT: for the patch you are writing.

EOT
	close(C);

	my $editor = $ENV{GIT_EDITOR} || $repo->config("core.editor") || $ENV{VISUAL} || $ENV{EDITOR} || "vi";
	system($editor, $compose_filename);

	open(C2,">",$compose_filename . ".final")
		or die "Failed to open $compose_filename.final : " . $!;

	open(C,"<",$compose_filename)
		or die "Failed to open $compose_filename : " . $!;

	while(<C>) {
		next if m/^GIT: /;
		print C2 $_;
	}
	close(C);
	close(C2);

	do {
		$_ = $term->readline("Send this email? (y|n) ");
	} while (!defined $_);

	if (uc substr($_,0,1) ne 'Y') {
		cleanup_compose_files();
		exit(0);
	}

	@files = ($compose_filename . ".final");
}


# Now that all the defaults are set, process the rest of the command line
# arguments and collect up the files that need to be processed.
for my $f (@ARGV) {
	if (-d $f) {
		opendir(DH,$f)
			or die "Failed to opendir $f: $!";

		push @files, grep { -f $_ } map { +$f . "/" . $_ }
				sort readdir(DH);

	} elsif (-f $f) {
		push @files, $f;

	} else {
		print STDERR "Skipping $f - not found.\n";
	}
}

if (@files) {
	unless ($quiet) {
		print $_,"\n" for (@files);
	}
} else {
	print STDERR "\nNo patch files specified!\n\n";
	usage();
}

# Variables we set as part of the loop over files
our ($message_id, %mail, $subject, $reply_to, $references, $message);

sub extract_valid_address {
	my $address = shift;
	my $local_part_regexp = '[^<>"\s@]+';
	my $domain_regexp = '[^.<>"\s@]+(?:\.[^.<>"\s@]+)+';

	# check for a local address:
	return $address if ($address =~ /^($local_part_regexp)$/);

	$address =~ s/^\s*<(.*)>\s*$/$1/;
	if ($have_email_valid) {
		return scalar Email::Valid->address($address);
	} else {
		# less robust/correct than the monster regexp in Email::Valid,
		# but still does a 99% job, and one less dependency
		$address =~ /($local_part_regexp\@$domain_regexp)/;
		return $1;
	}
}

# Usually don't need to change anything below here.

# we make a "fake" message id by taking the current number
# of seconds since the beginning of Unix time and tacking on
# a random number to the end, in case we are called quicker than
# 1 second since the last time we were called.

# We'll setup a template for the message id, using the "from" address:

my ($message_id_stamp, $message_id_serial);
sub make_message_id
{
	my $uniq;
	if (!defined $message_id_stamp) {
		$message_id_stamp = sprintf("%s-%s", time, $$);
		$message_id_serial = 0;
	}
	$message_id_serial++;
	$uniq = "$message_id_stamp-$message_id_serial";

	my $du_part;
	for ($sender, $repocommitter, $repoauthor) {
		$du_part = extract_valid_address(sanitize_address($_));
		last if (defined $du_part and $du_part ne '');
	}
	if (not defined $du_part or $du_part eq '') {
		use Sys::Hostname qw();
		$du_part = 'user@' . Sys::Hostname::hostname();
	}
	my $message_id_template = "<%s-git-send-email-%s>";
	$message_id = sprintf($message_id_template, $uniq, $du_part);
	#print "new message id = $message_id\n"; # Was useful for debugging
}



$time = time - scalar $#files;

sub unquote_rfc2047 {
	local ($_) = @_;
	my $encoding;
	if (s/=\?([^?]+)\?q\?(.*)\?=/$2/g) {
		$encoding = $1;
		s/_/ /g;
		s/=([0-9A-F]{2})/chr(hex($1))/eg;
	}
	return wantarray ? ($_, $encoding) : $_;
}

# use the simplest quoting being able to handle the recipient
sub sanitize_address
{
	my ($recipient) = @_;
	my ($recipient_name, $recipient_addr) = ($recipient =~ /^(.*?)\s*(<.*)/);

	if (not $recipient_name) {
		return "$recipient";
	}

	# if recipient_name is already quoted, do nothing
	if ($recipient_name =~ /^(".*"|=\?utf-8\?q\?.*\?=)$/) {
		return $recipient;
	}

	# rfc2047 is needed if a non-ascii char is included
	if ($recipient_name =~ /[^[:ascii:]]/) {
		$recipient_name =~ s/([^-a-zA-Z0-9!*+\/])/sprintf("=%02X", ord($1))/eg;
		$recipient_name =~ s/(.*)/=\?utf-8\?q\?$1\?=/;
	}

	# double quotes are needed if specials or CTLs are included
	elsif ($recipient_name =~ /[][()<>@,;:\\".\000-\037\177]/) {
		$recipient_name =~ s/(["\\\r])/\\$1/;
		$recipient_name = "\"$recipient_name\"";
	}

	return "$recipient_name $recipient_addr";

}

sub send_message
{
	my @recipients = unique_email_list(@to);
	@cc = (map { sanitize_address($_) } @cc);
	my $to = join (",\n\t", @recipients);
	@recipients = unique_email_list(@recipients,@cc,@bcclist);
	@recipients = (map { extract_valid_address($_) } @recipients);
	my $date = format_2822_time($time++);
	my $gitversion = '@@GIT_VERSION@@';
	if ($gitversion =~ m/..GIT_VERSION../) {
	    $gitversion = Git::version();
	}

	my $cc = join(", ", unique_email_list(@cc));
	my $ccline = "";
	if ($cc ne '') {
		$ccline = "\nCc: $cc";
	}
	my $sanitized_sender = sanitize_address($sender);
	make_message_id() unless defined($message_id);

	my $header = "From: $sanitized_sender
To: $to${ccline}
Subject: $subject
Date: $date
Message-Id: $message_id
X-Mailer: git-send-email $gitversion
";
	if ($thread && $reply_to) {

		$header .= "In-Reply-To: $reply_to\n";
		$header .= "References: $references\n";
	}
	if (@xh) {
		$header .= join("\n", @xh) . "\n";
	}

	my @sendmail_parameters = ('-i', @recipients);
	my $raw_from = $sanitized_sender;
	$raw_from = $envelope_sender if (defined $envelope_sender);
	$raw_from = extract_valid_address($raw_from);
	unshift (@sendmail_parameters,
			'-f', $raw_from) if(defined $envelope_sender);

	if ($dry_run) {
		# We don't want to send the email.
	} elsif ($smtp_server =~ m#^/#) {
		my $pid = open my $sm, '|-';
		defined $pid or die $!;
		if (!$pid) {
			exec($smtp_server, @sendmail_parameters) or die $!;
		}
		print $sm "$header\n$message";
		close $sm or die $?;
	} else {
		require Net::SMTP;
		$smtp ||= Net::SMTP->new( $smtp_server );
		$smtp->mail( $raw_from ) or die $smtp->message;
		$smtp->to( @recipients ) or die $smtp->message;
		$smtp->data or die $smtp->message;
		$smtp->datasend("$header\n$message") or die $smtp->message;
		$smtp->dataend() or die $smtp->message;
		$smtp->ok or die "Failed to send $subject\n".$smtp->message;
	}
	if ($quiet) {
		printf (($dry_run ? "Dry-" : "")."Sent %s\n", $subject);
	} else {
		print (($dry_run ? "Dry-" : "")."OK. Log says:\nDate: $date\n");
		if ($smtp_server !~ m#^/#) {
			print "Server: $smtp_server\n";
			print "MAIL FROM:<$raw_from>\n";
			print "RCPT TO:".join(',',(map { "<$_>" } @recipients))."\n";
		} else {
			print "Sendmail: $smtp_server ".join(' ',@sendmail_parameters)."\n";
		}
		print "From: $sanitized_sender\nSubject: $subject\nCc: $cc\nTo: $to\n\n";
		if ($smtp) {
			print "Result: ", $smtp->code, ' ',
				($smtp->message =~ /\n([^\n]+\n)$/s), "\n";
		} else {
			print "Result: OK\n";
		}
	}
}

$reply_to = $initial_reply_to;
$references = $initial_reply_to || '';
$subject = $initial_subject;

foreach my $t (@files) {
	open(F,"<",$t) or die "can't open file $t";

	my $author = undef;
	my $author_encoding;
	my $has_content_type;
	my $body_encoding;
	@cc = @initial_cc;
	@xh = ();
	my $input_format = undef;
	my $header_done = 0;
	$message = "";
	while(<F>) {
		if (!$header_done) {
			if (/^From /) {
				$input_format = 'mbox';
				next;
			}
			chomp;
			if (!defined $input_format && /^[-A-Za-z]+:\s/) {
				$input_format = 'mbox';
			}

			if (defined $input_format && $input_format eq 'mbox') {
				if (/^Subject:\s+(.*)$/) {
					$subject = $1;

				} elsif (/^(Cc|From):\s+(.*)$/) {
					if (unquote_rfc2047($2) eq $sender) {
						next if ($suppress_from);
					}
					elsif ($1 eq 'From') {
						($author, $author_encoding)
						  = unquote_rfc2047($2);
					}
					printf("(mbox) Adding cc: %s from line '%s'\n",
						$2, $_) unless $quiet;
					push @cc, $2;
				}
				elsif (/^Content-type:/i) {
					$has_content_type = 1;
					if (/charset="?[^ "]+/) {
						$body_encoding = $1;
					}
					push @xh, $_;
				}
				elsif (/^Message-Id: (.*)/i) {
					$message_id = $1;
				}
				elsif (!/^Date:\s/ && /^[-A-Za-z]+:\s+\S/) {
					push @xh, $_;
				}

			} else {
				# In the traditional
				# "send lots of email" format,
				# line 1 = cc
				# line 2 = subject
				# So let's support that, too.
				$input_format = 'lots';
				if (@cc == 0) {
					printf("(non-mbox) Adding cc: %s from line '%s'\n",
						$_, $_) unless $quiet;

					push @cc, $_;

				} elsif (!defined $subject) {
					$subject = $_;
				}
			}

			# A whitespace line will terminate the headers
			if (m/^\s*$/) {
				$header_done = 1;
			}
		} else {
			$message .=  $_;
			if (/^(Signed-off-by|Cc): (.*)$/i && $signed_off_cc) {
				my $c = $2;
				chomp $c;
				push @cc, $c;
				printf("(sob) Adding cc: %s from line '%s'\n",
					$c, $_) unless $quiet;
			}
		}
	}
	close F;

	if ($cc_cmd ne "") {
		open(F, "$cc_cmd $t |")
			or die "(cc-cmd) Could not execute '$cc_cmd'";
		while(<F>) {
			my $c = $_;
			$c =~ s/^\s*//g;
			$c =~ s/\n$//g;
			push @cc, $c;
			printf("(cc-cmd) Adding cc: %s from: '%s'\n",
				$c, $cc_cmd) unless $quiet;
		}
		close F
			or die "(cc-cmd) failed to close pipe to '$cc_cmd'";
	}

	if (defined $author) {
		$message = "From: $author\n\n$message";
		if (defined $author_encoding) {
			if ($has_content_type) {
				if ($body_encoding eq $author_encoding) {
					# ok, we already have the right encoding
				}
				else {
					# uh oh, we should re-encode
				}
			}
			else {
				push @xh,
				  'MIME-Version: 1.0',
				  "Content-Type: text/plain; charset=$author_encoding",
				  'Content-Transfer-Encoding: 8bit';
			}
		}
	}

	send_message();

	# set up for the next message
	if ($chain_reply_to || !defined $reply_to || length($reply_to) == 0) {
		$reply_to = $message_id;
		if (length $references > 0) {
			$references .= "\n $message_id";
		} else {
			$references = "$message_id";
		}
	}
	$message_id = undef;
}

if ($compose) {
	cleanup_compose_files();
}

sub cleanup_compose_files() {
	unlink($compose_filename, $compose_filename . ".final");

}

$smtp->quit if $smtp;

sub unique_email_list(@) {
	my %seen;
	my @emails;

	foreach my $entry (@_) {
		if (my $clean = extract_valid_address($entry)) {
			$seen{$clean} ||= 0;
			next if $seen{$clean}++;
			push @emails, $entry;
		} else {
			print STDERR "W: unable to extract a valid address",
					" from: $entry\n";
		}
	}
	return @emails;
}
