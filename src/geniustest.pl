#!/usr/bin/perl

use warnings;
use strict;

open(my $tests_fh, "<", "geniustests.txt")
	|| die "can't open the geniustests.txt file";

my $errors = 0;
my $errorinputs = "";
my $tests = 0;
my $options = "";

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
	my $shd = $2 // "";

	$tests++;

	print "$command\n";
	$command =~ s/'/'\\''/g;
	open(my $genius_fh, "-|" ,"./genius --exec='$command' $options") ||
		die "can't open the genius process pipe!";

	if(my $rep=<$genius_fh>) {
		chomp $rep;

		print " (should be)=$shd\n";
		print " (reported)=$rep\n";
		if($rep ne $shd) {
			print "\e[01;31mERROR!\e[0m\n";
			$errors++;
			$errorinputs = $errorinputs . "\n$command";
		}
	} else {
		print " (should be)=$shd\n";
		print " (reported)=\n";
		if($shd ne "") {
			print "\e[01;31mERROR! NO OUTPUT\e[0m\n";
			$errors++;
			$errorinputs = $errorinputs . "\n$command";
		}
	}
	print "\n";
	close($genius_fh);
}

print "tests: $tests, errors: $errors\n";
if ($errors > 0) {
	print "Inputs with errors: $errorinputs\n";
}
