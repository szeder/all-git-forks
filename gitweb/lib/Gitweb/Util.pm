# Gitweb::Util -- Internal utilities used by gitweb (git web interface)
#                 that do not contain Git- or Gitweb-specific knowledge
#
# This module is licensed under the GPLv2

package Gitweb::Util;

use strict;
use warnings;
use Exporter qw(import);

our @EXPORT = qw(to_utf8
                 esc_param esc_path_info esc_url
                 esc_html esc_path esc_attr
                 sanitize untabify
                 chop_str chop_and_escape_str
                 esc_html_hl_regions matchpos_list
                 esc_html_match_hl esc_html_match_hl_chopped
                 hash_set_multi
                 get_file_owner insert_file normalize_link_target
                 $fallback_encoding);
our @EXPORT_OK = qw(quot_cec quot_upr);

use Encode;
use CGI;

# ......................................................................
# Perl encoding (utf-8)

# decode sequences of octets in utf8 into Perl's internal form,
# which is utf-8 with utf8 flag set if needed.  gitweb writes out
# in utf-8 thanks to "binmode STDOUT, ':utf8'" at beginning of gitweb.perl
our $fallback_encoding = 'latin1';
sub to_utf8 {
	my $str = shift;
	return undef unless defined $str;

	if (utf8::is_utf8($str) || utf8::decode($str)) {
		return $str;
	} else {
		return decode($fallback_encoding, $str, Encode::FB_DEFAULT);
	}
}

# ......................................................................
# CGI encoding

# quote unsafe chars, but keep the slash, even when it's not
# correct, but quoted slashes look too horrible in bookmarks
sub esc_param {
	my $str = shift;
	return undef unless defined $str;

	$str =~ s/([^A-Za-z0-9\-_.~()\/:@ ]+)/CGI::escape($1)/eg;
	$str =~ s/ /\+/g;

	return $str;
}

# the quoting rules for path_info fragment are slightly different
sub esc_path_info {
	my $str = shift;
	return undef unless defined $str;

	# path_info doesn't treat '+' as space (specially), but '?' must be escaped
	$str =~ s/([^A-Za-z0-9\-_.~();\/;:@&= +]+)/CGI::escape($1)/eg;

	return $str;
}

# quote unsafe chars in whole URL, so some characters cannot be quoted
sub esc_url {
	my $str = shift;
	return undef unless defined $str;

	$str =~ s/([^A-Za-z0-9\-_.~();\/;?:@&= ]+)/CGI::escape($1)/eg;
	$str =~ s/ /\+/g;

	return $str;
}

# ......................................................................
# (X)HTML escaping

# replace invalid utf8 character with SUBSTITUTION sequence
sub esc_html {
	my $str = shift;
	my %opts = @_;

	return undef unless defined $str;

	$str = to_utf8($str);
	$str = CGI::escapeHTML($str);
	if ($opts{'-nbsp'}) {
		$str =~ s/ /&nbsp;/g;
	}
	$str =~ s|([[:cntrl:]])|(($1 ne "\t") ? quot_cec($1) : $1)|eg;
	return $str;
}

# quote unsafe characters in HTML attributes
sub esc_attr {

	# for XHTML conformance escaping '"' to '&quot;' is not enough
	return esc_html(@_);
}

# quote control characters and escape filename to HTML
sub esc_path {
	my $str = shift;
	my %opts = @_;

	return undef unless defined $str;

	$str = to_utf8($str);
	$str = CGI::escapeHTML($str);
	if ($opts{'-nbsp'}) {
		$str =~ s/ /&nbsp;/g;
	}
	$str =~ s|([[:cntrl:]])|quot_cec($1)|eg;
	return $str;
}

# Sanitize for use in XHTML + application/xml+xhtm (valid XML 1.0)
sub sanitize {
	my $str = shift;

	return undef unless defined $str;

	$str = to_utf8($str);
	$str =~ s|([[:cntrl:]])|($1 =~ /[\t\n\r]/ ? $1 : quot_cec($1))|eg;
	return $str;
}

# ......................................................................
# Pretty-printing

# escape tabs (convert tabs to spaces)
sub untabify {
	my $line = shift;

	while ((my $pos = index($line, "\t")) != -1) {
		if (my $count = (8 - ($pos % 8))) {
			my $spaces = ' ' x $count;
			$line =~ s/\t/$spaces/;
		}
	}

	return $line;
}

# ......................................................................
# HTML aware string manipulation

# Try to chop given string on a word boundary between position
# $len and $len+$add_len. If there is no word boundary there,
# chop at $len+$add_len. Do not chop if chopped part plus ellipsis
# (marking chopped part) would be longer than given string.
sub chop_str {
	my $str = shift;
	my $len = shift;
	my $add_len = shift || 10;
	my $where = shift || 'right'; # 'left' | 'center' | 'right'

	# Make sure perl knows it is utf8 encoded so we don't
	# cut in the middle of a utf8 multibyte char.
	$str = to_utf8($str);

	# allow only $len chars, but don't cut a word if it would fit in $add_len
	# if it doesn't fit, cut it if it's still longer than the dots we would add
	# remove chopped character entities entirely

	# when chopping in the middle, distribute $len into left and right part
	# return early if chopping wouldn't make string shorter
	if ($where eq 'center') {
		return $str if ($len + 5 >= length($str)); # filler is length 5
		$len = int($len/2);
	} else {
		return $str if ($len + 4 >= length($str)); # filler is length 4
	}

	# regexps: ending and beginning with word part up to $add_len
	my $endre = qr/.{$len}\w{0,$add_len}/;
	my $begre = qr/\w{0,$add_len}.{$len}/;

	if ($where eq 'left') {
		$str =~ m/^(.*?)($begre)$/;
		my ($lead, $body) = ($1, $2);
		if (length($lead) > 4) {
			$lead = " ...";
		}
		return "$lead$body";

	} elsif ($where eq 'center') {
		$str =~ m/^($endre)(.*)$/;
		my ($left, $str)  = ($1, $2);
		$str =~ m/^(.*?)($begre)$/;
		my ($mid, $right) = ($1, $2);
		if (length($mid) > 5) {
			$mid = " ... ";
		}
		return "$left$mid$right";

	} else {
		$str =~ m/^($endre)(.*)$/;
		my $body = $1;
		my $tail = $2;
		if (length($tail) > 4) {
			$tail = "... ";
		}
		return "$body$tail";
	}
}

# takes the same arguments as chop_str, but also wraps a <span> around the
# result with a title attribute if it does get chopped. Additionally, the
# string is HTML-escaped.
sub chop_and_escape_str {
	my ($str) = @_;

	my $chopped = chop_str(@_);
	$str = to_utf8($str);
	if ($chopped eq $str) {
		return esc_html($chopped);
	} else {
		$str =~ s/[[:cntrl:]]/?/g;
		return CGI::span({-title=>$str}, esc_html($chopped));
	}
}

# Highlight selected fragments of string, using given CSS class,
# and escape HTML.  It is assumed that fragments do not overlap.
# Regions are passed as list of pairs (array references).
#
# Example: esc_html_hl_regions("foobar", "mark", [ 0, 3 ]) returns
# '<span class="mark">foo</span>bar'
sub esc_html_hl_regions {
	my ($str, $css_class, @sel) = @_;
	return esc_html($str) unless @sel;

	my $out = '';
	my $pos = 0;

	for my $s (@sel) {
		$out .= esc_html(substr($str, $pos, $s->[0] - $pos))
			if ($s->[0] - $pos > 0);
		$out .= CGI::span({-class => $css_class},
		                  esc_html(substr($str, $s->[0], $s->[1] - $s->[0])));

		$pos = $s->[1];
	}
	$out .= esc_html(substr($str, $pos))
		if ($pos < length($str));

	return $out;
}

# return positions of beginning and end of each match
sub matchpos_list {
	my ($str, $regexp) = @_;
	return unless (defined $str && defined $regexp);

	my @matches;
	while ($str =~ /$regexp/g) {
		push @matches, [$-[0], $+[0]];
	}
	return @matches;
}

# highlight match (if any), and escape HTML
sub esc_html_match_hl {
	my ($str, $regexp) = @_;
	return esc_html($str) unless defined $regexp;

	my @matches = matchpos_list($str, $regexp);
	return esc_html($str) unless @matches;

	return esc_html_hl_regions($str, 'match', @matches);
}


# highlight match (if any) of shortened string, and escape HTML
sub esc_html_match_hl_chopped {
	my ($str, $chopped, $regexp) = @_;
	return esc_html_match_hl($str, $regexp) unless defined $chopped;

	my @matches = matchpos_list($str, $regexp);
	return esc_html($chopped) unless @matches;

	# filter matches so that we mark chopped string
	my $tail = "... "; # see chop_str
	unless ($chopped =~ s/\Q$tail\E$//) {
		$tail = '';
	}
	my $chop_len = length($chopped);
	my $tail_len = length($tail);
	my @filtered;

	for my $m (@matches) {
		if ($m->[0] > $chop_len) {
			push @filtered, [ $chop_len, $chop_len + $tail_len ] if ($tail_len > 0);
			last;
		} elsif ($m->[1] > $chop_len) {
			push @filtered, [ $m->[0], $chop_len + $tail_len ];
			last;
		}
		push @filtered, $m;
	}

	return esc_html_hl_regions($chopped . $tail, 'match', @filtered);
}

# ......................................................................
# Data structures manipulation

# store multiple values for single key as anonymous array reference
# single values stored directly in the hash, not as [ <value> ]
sub hash_set_multi {
	my ($hash, $key, $value) = @_;

	if (!exists $hash->{$key}) {
		$hash->{$key} = $value;
	} elsif (!ref $hash->{$key}) {
		$hash->{$key} = [ $hash->{$key}, $value ];
	} else {
		push @{$hash->{$key}}, $value;
	}
}

# ......................................................................
# filesystem-related functions

sub get_file_owner {
	my $path = shift;

	my ($dev, $ino, $mode, $nlink, $st_uid, $st_gid, $rdev, $size) = stat($path);
	my ($name, $passwd, $uid, $gid, $quota, $comment, $gcos, $dir, $shell) = getpwuid($st_uid);
	if (!defined $gcos) {
		return undef;
	}
	my $owner = $gcos;
	$owner =~ s/[,;].*$//;
	return to_utf8($owner);
}

# assume that file exists
sub insert_file {
	my $filename = shift;

	open my $fd, '<', $filename;
	print map { to_utf8($_) } <$fd>;
	close $fd;
}

# given link target, and the directory (basedir) the link is in,
# return target of link relative to top directory (top tree);
# return undef if it is not possible (including absolute links).
sub normalize_link_target {
	my ($link_target, $basedir) = @_;

	# absolute symlinks (beginning with '/') cannot be normalized
	return if (substr($link_target, 0, 1) eq '/');

	# normalize link target to path from top (root) tree (dir)
	my $path;
	if ($basedir) {
		$path = $basedir . '/' . $link_target;
	} else {
		# we are in top (root) tree (dir)
		$path = $link_target;
	}

	# remove //, /./, and /../
	my @path_parts;
	foreach my $part (split('/', $path)) {
		# discard '.' and ''
		next if (!$part || $part eq '.');
		# handle '..'
		if ($part eq '..') {
			if (@path_parts) {
				pop @path_parts;
			} else {
				# link leads outside repository (outside top dir)
				return;
			}
		} else {
			push @path_parts, $part;
		}
	}
	$path = join('/', @path_parts);

	return $path;
}

# ----------------------------------------------------------------------
# ......................................................................
# Showing "unprintable" characters (utility functions)

# Make control characters "printable", using character escape codes (CEC)
sub quot_cec {
	my $cntrl = shift;
	my %opts = @_;
	my %es = ( # character escape codes, aka escape sequences
		"\t" => '\t',   # tab            (HT)
		"\n" => '\n',   # line feed      (LF)
		"\r" => '\r',   # carrige return (CR)
		"\f" => '\f',   # form feed      (FF)
		"\b" => '\b',   # backspace      (BS)
		"\a" => '\a',   # alarm (bell)   (BEL)
		"\e" => '\e',   # escape         (ESC)
		"\013" => '\v', # vertical tab   (VT)
		"\000" => '\0', # nul character  (NUL)
	);
	my $chr = ( (exists $es{$cntrl})
		    ? $es{$cntrl}
		    : sprintf('\%2x', ord($cntrl)) );
	if ($opts{-nohtml}) {
		return $chr;
	} else {
		return "<span class=\"cntrl\">$chr</span>";
	}
}

# Alternatively use unicode control pictures codepoints,
# Unicode "printable representation" (PR)
sub quot_upr {
	my $cntrl = shift;
	my %opts = @_;

	my $chr = sprintf('&#%04d;', 0x2400+ord($cntrl));
	if ($opts{-nohtml}) {
		return $chr;
	} else {
		return "<span class=\"cntrl\">$chr</span>";
	}
}

1;
