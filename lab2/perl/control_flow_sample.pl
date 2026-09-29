#!/usr/bin/perl
use strict;
use warnings;
use feature 'switch';

my $value = 0;

for (my $i = 0; $i < 2; ++$i) {
    ++$value;
}
foreach my $item (1, 2) {
    $value += $item;
}
while ($value < 6) {
    ++$value;
}
until ($value > 6) {
    ++$value;
}
do {
    ++$value;
} while ($value < 9);
do {
    ++$value;
} until ($value >= 10);

my $comparison;
if ($value > 10) {
    $comparison = 'greater than ten';
} elsif ($value == 10) {
    $comparison = 'ten';
} else {
    $comparison = 'less than ten';
}
$comparison .= ', nonzero' unless $value == 0;
my $parity = $value % 2 ? 'odd' : 'even';
my $remainder;

{
    no warnings 'deprecated';
    given ($value % 4) {
        when (1) { $remainder = 'remainder 1'; }
        when (2) { $remainder = 'remainder 2'; }
        when (3) { $remainder = 'remainder 3'; }
        default  { $remainder = 'divisible by 4'; }
    }
}

my $result = "$comparison; $remainder; value=$value parity=$parity";
print "$result\n";
