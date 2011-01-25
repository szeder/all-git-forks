# Gitweb::Util -- Internal utilities used by gitweb (git web interface)
#
# This module is licensed under the GPLv2

package Gitweb::Util;

use strict;
use warnings;
use Exporter qw(import);

our @EXPORT = qw(to_utf8
                 esc_param esc_path_info esc_url
                 esc_html esc_path esc_attr
                 untabify
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
	if (utf8::valid($str)) {
		utf8::decode($str);
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

# ......................................................................
# Other

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

# ----------------------------------------------------------------------
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
