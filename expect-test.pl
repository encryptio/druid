#!/usr/bin/perl
use warnings;
use strict;

my ($dir) = @ARGV;
die unless -d $dir;

my $lf;
if ( -e "$dir/go.lua" ) {
    open $lf, "-|", "prog/druid", "$dir/go.lua"
        or die "Couldn't open pipe to druid: $!";
} elsif ( -e "$dir/go.sh" ) {
    open $lf, "-|", "cd \Q$dir\E; sh go.sh"
        or die "Couldn't open pipe to shell: $!";
}

open my $lf2, "<", "$dir/expect"
    or die "Couldn't open $dir/expect: $!";
my $expect = do { local $/; <$lf2> };
close $lf2;

my $got = do { local $/; <$lf> };

close $lf;
die "prog/druid exited with code ".($? >> 8) if $?;

if ( $got ne $expect ) {
    print "Didn't get what was expected.\n";
    open my $sf, ">", "$dir/actual-out"
        or die "Couldn't open $dir/actual-out for writing: $!";
    print $sf $got;
    close $sf;
    print "Actual output written to $dir/actual-out.\n";
    exit(1);
}

# all okay
exit(0);

