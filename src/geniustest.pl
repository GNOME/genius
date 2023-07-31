#!/usr/bin/perl

use warnings;
use strict;

open(my $tests_fh, "<", "geniustests.txt")
	|| die "can't open the geniustests.txt file";

my $i = 0;
my $errors = 0;
my $errorinputs = "";
my $tests = 0;
my $options = "";

while(<$tests_fh>) {

	my $command;
	my $shd;

	if(/^OPTIONS[ 	]+(.*)$/) {
		$options = $1;
		next;
	} elsif(/^([^	]+)	+([^	]+)$/) {
		$tests++;
		$command = $1;
		$shd=$2;
	} elsif(/^([^	]+)$/) {
		$tests++;
		$command = $1;
		$shd="";
	} else {
		next;
	}

	print "$1\n";
	#something weird happens and the following modifies $1 and $2
	#as well, I guess those can only be used from the last regexp
	$command =~ s/'/'\\''/g;
	open(my $genius_fh, "-|" ,"./genius --exec='$command' $options") ||
		die "can't open the genius process pipe!";

	if(my $rep=<$genius_fh>) {
		chomp $shd;
		chomp $rep;

		print " (should be)=$shd\n";
		print " (reported)=$rep\n";
		if($rep ne $shd) {
			print "\e[01;31mERROR!\e[0m\n";
			$errors++;
			$errorinputs = $errorinputs . "\n$command";
		}
	} else {
		chomp $shd;
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
	system("mv gmon.out gmon${i}.out") if(-e "gmon.out");
	$i++;
}

print "tests: $tests, errors: $errors\n";
if ($errors > 0) {
	print "Inputs with errors: $errorinputs\n";
}
