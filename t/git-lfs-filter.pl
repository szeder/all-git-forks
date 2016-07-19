#!/usr/bin/env perl

use strict;
use warnings;

sub smudge {
    my ($x) = @_;
    $x =~ 'y/N-ZA-Mn-za-m/A-Za-z/';    # rot13
    return $x;
}

sub clean {
    my ($x) = @_;
    $x =~ 'y/A-Za-z/N-ZA-Mn-za-m/';
    return $x;
}

sub say {

    # modern versions of perl have this built in.
    print "$_[0]\n";
}

sub filter_command {
    my ( $command, $file_name_len, $file_name, $file_content_len,
        $file_content );
    binmode STDIN;
    binmode STDOUT;

    {
        my $buf;
        my $bytes_read = 0;

        # get command (clean or smudge)
        while ( $bytes_read < 4 ) {
            my $num_bytes = sysread STDIN, $buf, 4;

            unless ( defined($num_bytes) ) {
                say "error: $! => could not read command";
                exit(1);
            }

            if ( $num_bytes == 0 ) {
                say "premature EOF reading command";
                exit(1);
            }
            else {
                $bytes_read += $num_bytes;
            }

        }
        $command = unpack( "I", $buf );
    }

    {
        my $buf;
        my $bytes_read = 0;

        while ( $bytes_read < 4 ) {
            my $num_bytes = sysread STDIN, $buf, 4;

            unless ( defined($num_bytes) ) {
                say "error: $! => could not determine filename length";
                exit(1);
            }

            if ( $num_bytes == 0 ) {
                say "premature EOF reading file name length";
                exit(1);
            }
            else {
                $bytes_read += $num_bytes;
            }

        }

        $file_name_len = unpack( "I", $buf );
        unless ( $file_name_len > 0 ) {
            say "zero file name length: no file?";
            exit(1);
        }
    }

    {
        my $buf;
        my $bytes_read = 0;

        while ( $bytes_read < $file_name_len ) {

            my $num_bytes = sysread STDIN, $buf, $file_name_len;

            unless ( defined($num_bytes) ) {
                say "error: $! => could not determine filename";
                exit(1);
            }

            if ( $num_bytes == 0 ) {
                say "premature EOF reading file name";
                exit(1);
            }
            else {
                $bytes_read += $num_bytes;
            }

        }
        $file_name = unpack( "A$file_name_len", $buf );
    }

    {
        my $buf;
        my $bytes_read = 0;

        while ( $bytes_read < 4 ) {
            my $num_bytes = sysread STDIN, $buf, 4;

            unless ( defined($num_bytes) ) {
                say "error: $! => could not read pointer length";
                exit(1);
            }

            if ( $num_bytes == 0 ) {
                say "premature EOF reading pointer length";
                exit(1);
            }
            else {
                $bytes_read += $num_bytes;
            }
        }
        $file_content_len = unpack( "I", $buf );
        unless ( $file_content_len > 0 ) {
            say "zero lfs pointer length";
            exit(1);
        }
    }

    {
        my $buf;
        my $bytes_read = 0;

        while ( $bytes_read < $file_content_len ) {

            my $num_bytes = sysread STDIN, $buf, $file_name_len;

            unless ( defined($num_bytes) ) {
                say "error: $! => could not determine filename";
                exit(1);
            }

            if ( $bytes_read == 0 ) {
                last;
            }

            $bytes_read += $num_bytes;
        }
        $file_content = unpack( "A$file_content_len", $buf );
    }

    my $out;
    if ( $command == 1 ) {
        $out = clean( $file_content, $file_name );
    }
    elsif ( $command == 2 ) {
        $out = smudge( $file_content, $file_name );
    }
    else {
        say "invalid command";
        exit(1);
    }

    my $out_len = length($out);

    {
        my $bytes_written = 0;
        while ( $bytes_written < 4 ) {
            my $num_bytes = syswrite STDOUT, pack( "I", $out_len );

            unless ( defined($num_bytes) ) {
                say "error: $! => could not write length of output";
                exit(1);
            }
            $bytes_written += $num_bytes;
        }
    }

    {
        my $bytes_written = 0;
        while ( $bytes_written < $out_len ) {
            my $num_bytes = syswrite STDOUT, pack( "A$out_len", $out );

            unless ( defined($num_bytes) ) {
                say "error: $! => could not write length of output";
                exit(1);
            }
            $bytes_written += $num_bytes;

        }
    }
}

filter_command();
