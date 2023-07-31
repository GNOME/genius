#!/usr/bin/perl

use warnings;
use strict;

open(my $tests_fh, "<", "geniustests.txt")
	|| die "can't open the geniustests.txt file";

my $errors = 0;
my $errorinputs = "";
my $tests = 0;
my $options = "";

my $colours = -t STDOUT;

sub cprintf {
	my ($n, $m) = @_;
	if ($colours) {
		printf "\e[01;%im%s\e[0m\n", $n, $m;
		return;
	}
	printf "%s\n", $m;
}

while (my $line = <$tests_fh>) {

	next unless $line =~ /
		^
		\s*						# in case there happens to be whitespace
		([^\t]+)			# expression or keyword
		(?:\t+|$)			# either the tab separator, or that's it
		(.*?)					# expected result or keyword complement
		\s*						# minus the whitespace if any
		$
		/x;

	if ($1 eq "OPTIONS") {
		$options = $2;
		next;
	}

	my $command = $1;
	my $expected = $2 // "";

	$tests++;

	cprintf 34, $command;
	$command =~ s/'/'\\''/g;

	open(my $genius_fh, "-|" ,"./genius --exec='$command' $options") ||
		die "can't open the genius process pipe!";

	my $result = "ERROR!";
	my $cc = 31; # colour code
	my $returned = <$genius_fh> // "";
	chomp $returned;

	if ($returned ne $expected) {
		$errors++;
		$errorinputs = $errorinputs . "\n$command";
		$result = "ERROR! NO OUTPUT" if $returned eq "";
	} else {
		$result = "OK";
		$cc = 32;
	}

	cprintf 33, "expected: $expected";
	cprintf 33, "returned: $returned";
	cprintf $cc, $result;
	print "\n";

	close($genius_fh);
}

print "tests: $tests, errors: $errors\n";
if ($errors > 0) {
	print "Inputs with errors: $errorinputs\n";
}
