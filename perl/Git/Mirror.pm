
=head1 NAME

Git::Mirror - automatic mirroring back-end state

=head1 SYNOPSIS

  use Git::Mirror;

  my $mirror = Git::Mirror->next_mirror;

  $mirror = Git::Mirror->find( );

=head1 DESCRIPTION

Right now this is just a placeholder for ideas about what information
needs to be stored to implement the various B<git mirror>
sub-commands.

=head1 STATE

State is stored in the config file; though care is taken to avoid 

=head2 MIRROR LISTS

The important things we need to know about a mirror:

=over

=item string URL

The public address of this mirror, also possibly the master address of
a particular person's fork of the project.

=item list of (keyid, stratum, timestamp) tracked

This indicates that this mirror is mirroring pushes from B<keyid>, and
for that key it has a given B<stratum> level, the most current I<push
tag> being dated with B<timestamp>.

=item int last_checked

How long ago it was this node last successfully called the mirror.

=item int ttl

Time since C<last_checked> we keep advertising this address to others.

=item int expire

Time from unsuccessful contact until we delete the address from our
records.

=item struct transfer_stats

Total bytes and bundles transferred, overall and rolling performance
statistics.

=back

=cut

package Git::Mirror;

my $config_obj

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
