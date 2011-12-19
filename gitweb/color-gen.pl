#!/usr/bin/perl -w
my $min_L = 60;
my $n_select = 200;
my $n_consider = 20;
my $n_dist_compare = 3;
my $n_remove = 0.5;
use strict;

$| = 1;
my $rand_seed = time ^ $$;
srand($rand_seed);

# n_select=100:
# seed 1323967799: sum = 3583.25; 5%=84.00, 10%=55.30, 25%=34.94, 50%=25.39, 75%=21.77, 90%=20.38, min=15.47
# seed 1323968264: sum = 3628.58; 5%=84.12, 10%=57.22, 25%=34.78, 50%=25.88, 75%=21.58, 90%=20.37, min=17.72
#
# n_select=200:
# seed 1323968256: sum = 5252.46; 5%=58.91, 10%=40.40, 25%=25.59, 50%=18.63, 75%=16.48, 90%=15.19, min=11.87
# seed 1323968374: sum = 5297.95; 5%=57.56, 10%=40.24, 25%=25.55, 50%=18.82, 75%=16.42, 90%=14.89, min=13.52
# seed 1323970080: sum = 5302.66; 5%=59.98, 10%=40.66, 25%=25.75, 50%=18.95, 75%=16.41, 90%=15.24, min=13.75
# seed 1323966948: sum = 5305.05; 5%=60.21, 10%=40.93, 25%=25.74, 50%=18.97, 75%=16.18, 90%=15.01, min=13.86
#
# http://www.ece.rochester.edu/~gsharma/ciede2000/

use lib '/tmp/Graphics-ColorObject-0.5.0';
use ColorObject;
use Data::Dumper;
use FileHandle;
use POSIX;

my @selected = (Graphics::ColorObject->new_RGB([0,1,1], space => 'sRGB')->as_Lab());
my $remove = 0;
while (@selected < $n_select) {
    my @consider = getConsider();

    my @dists = map { getMinDist($_, \@selected, $n_dist_compare) } @consider;
    @dists = sort { $a->[1] <=> $b->[1] } @dists;
    my $keep = pop(@dists);
    if (0) {
        print "Keep: $keep->[1]\n";
    } else {
        $keep = hillClimb($keep, \@selected);
    }
    push (@selected, $keep->[0]);
    if (@selected > $n_select * 0.25) {
        $remove += $n_remove;
        if ($remove >= 1) {
            --$remove;
            # + 1 because we will find ourselves at distance 0.
            @dists = map { getMinDist($_, \@selected, $n_dist_compare + 1) } @selected;
            @dists = sort { $b->[1] <=> $a->[1] } @dists;
            print "Remove $dists[$#dists]->[1]; $#dists remain\n";
            pop @dists;
            @selected = map { $_->[0] } @dists;
        }
    }
    # print "------------------------\n";
}

{
    my $sum_d = 0;
    print "Selecting by distance: ";
    my @ordered_dists;
    my @dists = map { getMinDist($_, \@selected, $n_dist_compare+1) } @selected;
    @dists = sort { $a->[1] <=> $b->[1] } @dists;
    @selected = pop(@dists)->[0];
    my @pending = map { $_->[0] } @dists;
    while (@pending > 0) {
        @dists = map { getMinDist($_, \@selected, $n_dist_compare) } @pending;
        @dists = sort { $a->[1] <=> $b->[1] } @dists;
        my $c = pop(@dists);
        push (@selected, $c->[0]);
        print "$c->[1] ";
        push (@ordered_dists, $c->[1]);
        $sum_d += $c->[1];
        @pending = map { $_->[0] } @dists;
    }
    print "\nseed $rand_seed: sum = $sum_d; 5%=$ordered_dists[$n_select * 0.05], 10%=$ordered_dists[$n_select * 0.1], 25%=$ordered_dists[$n_select * 0.25], 50%=$ordered_dists[$n_select * 0.5], 75%=$ordered_dists[$n_select * 0.75], 90%=$ordered_dists[$n_select * 0.9], min=$ordered_dists[$n_select-2]\n";
}

my $fh = new FileHandle ">colorlist.txt" or die "?";
foreach my $color (@selected) {
    print $fh join(" ", @{Graphics::ColorObject->new_Lab($color)
                             ->as_RGB255(space => 'sRGB')}), "\n";
}
close($fh);

$fh = new FileHandle ">colors.ppm" or die "?";
my $ncolumns = 50 * ($n_select / 10);
print $fh "P6 500 $ncolumns 255\n";
for(my $i = 0; $i < $n_select; $i += 10) {
    my @rgb = map { Graphics::ColorObject->new_Lab($_)->as_RGB255(space => 'sRGB') }
        @selected[$i .. ($i + 9)];
    my @pixels = map { pack("C3", @$_) x 50 } @rgb;
    map { print $fh join('', @pixels); } 1 .. 50;
}
close($fh);

sub getConsider {
    my @consider;
    while (@consider < $n_consider) {
        my $possible = Graphics::ColorObject
            ->new_RGB([rand(), rand(), rand()], space => 'sRGB')
                ->as_Lab();
        $possible = [ map { sprintf("%.2f", $_) } @$possible ];
        # print join(" ", @$possible), "\n";
        # Keep only light colors.
        # Could keep one half of red-green or yellow-blue colorblindness by pruning on
        # a/b in addition.
        push (@consider, $possible) if $possible->[0] >= $min_L;
    }
    return @consider;
}

sub getMinDist {
    my ($consider, $selected, $keep) = @_;

    my @dist;
    foreach my $color (@$selected) {
        my $dist = 0;
        for (my $i = 0; $i < 3; ++$i) {
            $dist += ($color->[$i] - $consider->[$i]) ** 2;
        }
        push (@dist, $dist);
    }
    @dist = sort { $a <=> $b } @dist;
    splice(@dist, $keep) if @dist >= $keep;
    my @min_dist = map { sprintf("%.2f", sqrt($_)); } @dist;
    my $sum = 0;
    map { $sum += $_ } @min_dist;
    my $mean = sprintf("%.2f", $sum / (scalar @min_dist));
    print join(" ", @{$consider}), " -> $mean (", join(" ", @min_dist), ")\n"
        if 0;
    return [ $consider, $mean ];
}

sub byColorOrder {
    my @dists = map { $a->[$_] - $b->[$_] } 0..2;
    @dists = sort { abs($b) <=> abs($a) } @dists;
    return $dists[0] <=> 0;
#    foreach my $eq_dist (qw/5 2 1/) {
#        return $a->[2] <=> $b->[2] if abs($a->[2] - $b->[2]) > $eq_dist;
#        return $a->[1] <=> $b->[1] if abs($a->[1] - $b->[1]) > $eq_dist;
#        return $a->[0] <=> $b->[0] if abs($a->[0] - $b->[0]) > $eq_dist;
#    }
}

sub hillClimb {
    my ($keep, $selected) = @_;

    print join(" ", @{$keep->[0]}), " -> " if 0;
    foreach my $climb (qw/16 8 4 2 1 0.5 0.25 0.125/) {
        for (my $axis = 0; $axis < 3; ++$axis) {
            $keep = hillClimbAxis($keep, $selected, $axis, $climb);
            $keep = hillClimbAxis($keep, $selected, $axis, -$climb);
        }
    }
    print join(" ", @{$keep->[0]}), "\n" if 0;
    return $keep;
}

sub in_sRGB {
    my ($new) = @_;

    my $rgb1 = Graphics::ColorObject->new_Lab($new)->as_RGB(space => 'sRGB');
    my $rgb2 = Graphics::ColorObject->new_Lab($new)->as_RGB(space => 'sRGB', clip => 1);
    for (my $i = 0; $i < 3; ++$i) {
        return 0 unless abs($rgb1->[$i] - $rgb2->[$i]) < 0.001;
    }
    return 1;
}

sub hillClimbAxis {
    my ($keep, $selected, $axis, $climb) = @_;

    while (1) {
        my @new = @{$keep->[0]};
        $new[$axis] += $climb;
        return $keep unless in_sRGB(\@new) && $new[0] >= $min_L;
        my $new = getMinDist(\@new, $selected, $n_dist_compare);
        if ($new->[1] > $keep->[1]) {
            $keep = $new;
        } else {
            return $keep;
        }
    }
}

