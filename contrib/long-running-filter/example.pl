#!/usr/bin/perl
#
# Example implementation for the Git filter protocol version 2
# See Documentation/gitattributes.txt, section "Filter Protocol"
#

use strict;
use warnings;

my $MAX_PACKET_CONTENT_SIZE = 65516;

sub packet_read {
    my $buffer;
    my $bytes_read = read STDIN, $buffer, 4;
    if ( $bytes_read == 0 ) {

        # EOF - Git stopped talking to us!
        exit();
    }
    elsif ( $bytes_read != 4 ) {
        die "invalid packet size '$bytes_read' field";
    }
    my $pkt_size = hex($buffer);
    if ( $pkt_size == 0 ) {
        return ( 1, "" );
    }
    elsif ( $pkt_size > 4 ) {
        my $content_size = $pkt_size - 4;
        $bytes_read = read STDIN, $buffer, $content_size;
        if ( $bytes_read != $content_size ) {
            die "invalid packet ($content_size expected; $bytes_read read)";
        }
        return ( 0, $buffer );
    }
    else {
        die "invalid packet size";
    }
}

sub packet_write {
    my ($packet) = @_;
    print STDOUT sprintf( "%04x", length($packet) + 4 );
    print STDOUT $packet;
    STDOUT->flush();
}

sub packet_flush {
    print STDOUT sprintf( "%04x", 0 );
    STDOUT->flush();
}

( packet_read() eq ( 0, "git-filter-client" ) ) || die "bad initialization";
( packet_read() eq ( 0, "version=2" ) )         || die "bad version";
( packet_read() eq ( 1, "" ) )                  || die "bad version end";

packet_write("git-filter-server\n");
packet_write("version=2\n");

( packet_read() eq ( 0, "clean=true" ) )  || die "bad capability";
( packet_read() eq ( 0, "smudge=true" ) ) || die "bad capability";
( packet_read() eq ( 1, "" ) )            || die "bad capability end";

packet_write( "clean=true\n" );
packet_write( "smudge=true\n" );
packet_flush();

while (1) {
    my ($command) = packet_read() =~ /^command=([^=]+)\n$/;
    my ($pathname) = packet_read() =~ /^pathname=([^=]+)\n$/;

    packet_read();

    my $input = "";
    {
        binmode(STDIN);
        my $buffer;
        my $done = 0;
        while ( !$done ) {
            ( $done, $buffer ) = packet_read();
            $input .= $buffer;
        }
    }

    my $output;
    if ( $command eq "clean" ) {
        ### Perform clean here ###
        $output = $input;
    }
    elsif ( $command eq "smudge" ) {
        ### Perform smudge here ###
        $output = $input;
    }
    else {
        die "bad command '$command'";
    }

    packet_write("status=success\n");
    packet_flush();
    while ( length($output) > 0 ) {
        my $packet = substr( $output, 0, $MAX_PACKET_CONTENT_SIZE );
        packet_write($packet);
        if ( length($output) > $MAX_PACKET_CONTENT_SIZE ) {
            $output = substr( $output, $MAX_PACKET_CONTENT_SIZE );
        }
        else {
            $output = "";
        }
    }
    packet_flush(); # flush content!
    packet_flush(); # empty list!
}
