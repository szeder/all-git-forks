
package Git::Config;

use 5.006;
use strict;
use warnings;

use Git;
use Error qw(:try);

=head1 NAME

Git::Config - caching interface to git-config state

=head1 SYNOPSIS

  use Git::Config;

  my $conf = Git::Config->new( $git );

  # return single config items
  my $value = $conf->config( VARIABLE );

  # return multi-config items
  my @values = $conf->config( VARIABLE );

  # manipulate type of slot, for special interpretation
  $conf->type( VARIABLE => $type );

  # change value
  $conf->config( VARIABLE => $value );

  # read or configure a global or system variable
  $conf->global( VARIABLE => $value );
  $conf->system( VARIABLE => $value );

  # default is to autoflush 
  $conf->autoflush( 0 );

  # if autoflush is disabled, flush explicitly.
  $conf->flush;

  # update cache
  $conf->read;

=head1 DESCRIPTION

This module provides a cached interface to the B<git config> command
in Perl.

=head1 CONSTRUCTION

=head2 B<Git::Config-E<gt>new( I<$git> )>

Creates a new B<Git::Config> object.  Does not read the configuration
file yet.

=cut

sub new {
	my $class = shift;
	my $git = shift || Git->repository;
	my $self = bless { git => $git }, $class;
	while ( my ($item, $value) = splice @_, 0, 2 ) {
		try {
			$self->$item($value);
		}
		catch {
			throw Error::Simple (
				"Invalid constructor arg '$item'",
			       );
		};
	}
	return $self;
}

=head1 METHODS

=head2 B<$conf-E<gt>config( C<item> )>

Reads the value of a particular configuration item.
When called in B<list context>, returns all values of multiple value
items.
When called in B<scalar context>, raises an error if the item is
specified multiple times.

=head2 B<$conf-E<gt>config( C<item> =E<gt> I<$value> )>

Sets the value of an item.
If an B<array reference> is specified, the entire list of items is
replaced with the values specified.
If a single item is passed but the item is specified multiple times in
the configuration file, raises an error.
Returns the old value.

=cut

sub config {
	return $_[0]->_config("", @_[1..$#_]);
}

sub _config {
	my $self = shift;
	my $which = shift;
	my $item = shift;
	my $value_passed = @_;
	my $value = shift;

	my $type = $self->type( $item );

	my $_which = $which ? "${which}_" : "";
	my $_read = "read_${_which}state";
	my $_state = "${_which}state";

	if (!$self->{$_read}) {
		$self->read($which);
	}

	my $state;
	if (exists $self->{$_state}{$item}) {
		$state = $self->{$_state}{$item};
	}
	else {
		$state = $self->{$_read}{$item};
	}

	if ($value_passed) {
		if (!ref $value and defined $value and ref $state) {
			throw Error::Simple (
				"'$item' is specified multiple times",
			       );
		}
		if ( $type ) {
			if (ref $value) {
				$value = [ map { $self->freeze($type, $_) }
						   @$value ];
			}
			else {
				$value = $self->freeze($type, $value);
			}
		}
		$self->{$_state}{$item} = $value;
		$self->flush() if $self->autoflush;
	}

	if (defined wantarray) {
		my @values = ref $state ? @$state :
			defined $state ? ($state) : ();

		if ( my $type = $self->type( $item ) ) {
			@values = map { $self->thaw($type, $_) }
				@values;
		}

		if ( @values > 1 and
			     ( ($value_passed and not ref $value) or
				       (not $value_passed and not wantarray) )
			    ) {
			throw Error::Simple (
				"'$item' is specified multiple times",
			       );
		}

		return (wantarray ? @values : $values[0]);
	}
}

=head2 B<$conf-E<gt>read()>

Reads the current state of the configuration file.

=cut

sub read {
	my $self = shift;
	my $which = shift;

	my $git = $self->{git};

	my ($fh, $c) = $git->command_output_pipe(
		'config', ( $which ? ("--$which") : () ),
		'--list',
	       );
	my $read_state = {};

	while (<$fh>) {
		my ($item, $value) = split $_, "=", 2;
		my $exists = exists $read_state->{$item};
		my $sl = \( $read_state->{$item} );
		if (!$exists) {
			$$sl = $value;
		}
		elsif (!ref $$sl) {
			$$sl = [ $$sl, $value ];
		}
		else {
			push @{ $$sl }, $value;
		}
	}

	$git->command_close_pipe($fh, $c);

	if ( $which ) {
		$which .= "_";
	}
	else {
		$which = "";
	};

	$self->{"read_${which}state"} = $read_state;
}

=head2 type( VARIABLE => $type )

Specifies which set of rules to use when returning to and from the
config file.  C<$type> may be C<string>, C<boolean> or C<integer>.
Globs such as C<*> are allowed which match everything except C<.>
(full stop).

=head2 type( VARIABLE )

Returns the first matching defined type rule.

=cut

sub type {
	my $self = shift;
	my $item = shift;
	my $got_type = @_;
	my $type = shift;
	$type ||= "string";

	my $types = $self->{types} ||= [];
	if ( $got_type ) {
		$item =~ s{([\.(?])}{\\$1}g;
		$item =~ s{\*}{[^\\.]*}g;
		$item = qr{^$item$};
		@$types = grep { $_->[0] ne $item }
			@$types;
		push @$types, [ $item, $type ];
	}
	else {
	type:
		for (@$types) {
			if ($item =~ m{$_->[0]}) {
				$type = $_->[1];
				last type;
			}
		}

		$type;
	}
}

=head2 global( VARIABLE )

=head2 system( VARIABLE )

Return the value of the given variable in the global or system
configuration (F<~/.gitconfig> and F</etc/gitconfig> on Unix).

=head2 global( VARIABLE => $value )

=head2 system( VARIABLE => $value )

Set the value of the given variable in the global or system
configuration.

=cut

sub global { return $_[0]->_config("global", @_[1..$#_]) }
sub system { return $_[0]->_config("system", @_[1..$#_]) }

=head2 autoflush

Returns 0 or 1 depending on whether autoflush is enabled.  Defaults to
1.

=head2 autoflush ( $value )

Set the value of autoflush.  If set to 1, flushes immediately.

=cut

sub autoflush {
	my $self = shift;
	if (@_) {
		$self->{autoflush} = shift;
	}
	not (defined $self->{autoflush} and not $self->{autoflush});
}

=head2 flush

Flushes all changed configuration values to the config file(s).

=cut

sub flush {
	my $self = shift;

	for my $which ("", "global", "system") {
		my $st = $which . ($which ? "_" : "") . "state";;
		my $read = "read_$st";
		if (my $new = delete $self->{$st}) {
			$self->_write($which, $new, $self->{$read});
			$self->read($which);
		}
	}
}

sub _write {
	my $self = shift;
	my $which = shift;
	my $state = shift;
	my $read_state = shift;

	my $git = $self->{git};

	while (my ($item, $value) = each %$state) {
		my $old_value = $read_state->{$item};
		my @cmd = ($which ? ("--$which") : () );
		my $type = $self->type($item);
		if ($type ne "string") {
			push @cmd, "--$type";
		}
		if (ref $value) {
			$git->command_oneline (
				"config", @cmd, "--replace-all",
				 $item, $value->[0],
			       );
			for my $i (1..$#$value) {
				$git->command_oneline (
					"config", @cmd, "--add",
					$item, $value->[$i],
				       );
			};
		}
		elsif (defined $value) {
			$git->command_oneline (
				"config", @cmd, $item, $value,
			       );
		}
		elsif ($read_state->{$item}) {
			$git->command_oneline (
				"config", "--unset-all", @cmd,
				$item, $value,
			       );
		}
		else {
			# nothing to do - already not in config
		}
	}
}

sub _dispatch {
	no strict 'refs';
	my $self = shift;
	my $func = shift;
	my $type = shift;
	my $sym = __PACKAGE__."::${type}::$func";
	defined &{$sym}
		or throw Error::Simple "Bad type '$type' in $func";
	&{$sym}(@_, $self);
}

sub freeze {
	my $self = shift;
	$self->_dispatch("freeze", @_);
}

sub thaw {
	my $self = shift;
	$self->_dispatch("thaw", @_);
}

{
	package Git::Config::string;
	sub freeze { shift }
	sub thaw   { shift }
}
{
	package Git::Config::integer;
	our @mul = ("", "k", "M", "G");
	sub freeze {
		my $val = shift;
		my $scale = 0;
		while ( (my $num = int($val/1024))*1024 == $val ) {
			$scale++;
		 	$val = $num;
			last if $scale == $#mul;
		}
		$val.$mul[$scale];
	}
	our $mul_re = qr/^(\d+)\s*${\( "(".join("|", @mul).")" )}$/i;
	sub thaw {
		my $val = shift;
		$val =~ m{$mul_re}
			or throw Error::Simple "Bad value for integer: '$val'";
		my $num = $1;
		if ($2) {
			my $scale = 0;
			do { $num = $num * 1024 }
				until (lc($mul[++$scale]) eq lc($2));
		}
		$num;
	}
}
{
	package Git::Config::boolean;
	our @true = qw(true yes 1);
	our @false = qw(false no 0);
	our $true_re = qr/^${\( "(".join("|", @true).")" )}$/i;
	our $false_re = qr/^${\( "(".join("|", @false).")" )}$/i;
	sub freeze {
		my $val = shift;
		if (!!$val) {
			$true[0];
		}
		else {
			$false[0];
		}
	}
	sub thaw {
		my $val = shift;
		if ($val =~ m{$true_re}) {
			1;
		}
		elsif ($val =~ m{$false_re}) {
			0;
		}
		else {
			throw Error::Simple "Bad value for boolean: '$val'";
		}
	}
}

1;

__END__

=head1 SEE ALSO

L<Git>

=head1 AUTHOR AND LICENSE

Copyright 2009, Sam Vilain, L<sam@vilain.net>.  All Rights Reserved.
This program is Free Software; you may use it under the terms of the
Perl Artistic License 2.0 or later, or the GPL v2 or later.

=cut

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
