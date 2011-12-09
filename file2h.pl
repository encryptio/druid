#!/usr/bin/perl
use warnings;
use strict;

my $data = do { local $/; <> };
my $len = length($data);
$data .= "\0";

my $name = 'data';
$name = $1 if $data =~ /!file2h-name!([^!]+)!/;

print "static unsigned char ".$name."[] = {\n";
my $first = 1;
for my $chunk ( split /(.{16})/s, $data ) {
    next unless length $chunk;

    my $str = ($first ? " " : ",");
    $first = 0;

    $str .= join ", ", map { "0x".unpack("H2",$_) } split //, $chunk;
    print "$str\n";
}
print "};\n";
#print "static int ".$name."_length = $len;\n";

