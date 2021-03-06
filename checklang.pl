#!/bin/perl -w

# Check NFORenum's language files
#
# Takes no arguments.
#
# Checks all *_<lang>.h files for agreement with the corresponding *_english.h.

use strict;

my %en;

sub getfmt($){
	local $_=$_[0];
	my ($ofs,$fmt)=(-1,"");
	while(($ofs = index $_,'%',$ofs+1)!=-1){
		substr($_,$ofs,3) =~ /(%[248]x)/ or substr($_,$ofs,2) =~ /(%.)/ or die;
		$fmt .= $1;
	}	return $fmt;
}

##########################################################################

print "Checking message strings.\nLoading english.\n";

open FILE, "< lang/message_english.h", or die "Could not open lang/message_english.h.\n";

my $prev;

while(<FILE>){
	s#//.*##;
	next unless /MESSAGE\(/ || /MESSAGE_EX/ || $prev;
	next if /BAD_STRING/;
	$_="$prev$_" if $prev;
	$prev=$_ and next if /\\\s$/;
	$prev='';
	/MESSAGE.*\(([0-9A-Z_]*),\s*"(.*)",.*\)/s or die "$.:Could not parse line. Does it need a trailing backslash?\n";
	$en{$1}=getfmt($2);
}

close FILE;


while(<lang/message*>){
	next if /english/;
	open FILE, "< $_" or die "Could not open $_.\n";
	/_(.*)\./;
	print "Checking $1.\n";
	my %tr=%en;
	while(<FILE>){
		s#//.*##;
		next unless /MESSAGE\(/ || /MESSAGE_EX/ || $prev;
		next if /BAD_STRING/;
		$_="$prev$_" if $prev;
		$prev=$_ and next if /\\\s$/;
		$prev='';
		/MESSAGE.*\(([0-9A-Z_]*),\s*"(.*)",.*\)/s or die "$.:Could not parse line. Does it need a trailing backslash?\n";
		print "$.:$1 is a duplicate or does not exist in English.\n" and next unless exists $tr{$1};
		print "$.:Format for $1 does not match English format.\n" unless getfmt($2) eq $tr{$1};
		delete $tr{$1};
	}
	print "$_ is not translated\n" for keys %tr;
	close FILE;
}

undef %en;

##########################################################################

print "\nChecking extra strings.\nLoading english.\n";

open FILE, "< lang/extra_english.h", or die "Could not open lang/extra_english.h.\n";

while(<FILE>){
	next unless /^EXTRA/;
	/EXTRA\(([A-Z0-9_]*),"(.*)"\)/ or die "$.:Could not parse line. Does it need a trailing backslash?\n";
	$en{$1}=getfmt($2);
}

close FILE;


while(<lang/extra*>){
	next if /english/;
	open FILE, "< $_" or die "Could not open $_.\n";
	/_(.*)\./;
	print "Checking $1.\n";
	my %tr=%en;
	while(<FILE>){
		next unless /^EXTRA/;
		/EXTRA\(([A-Z0-9_]*),"(.*)"\)/ or die "$.:Could not parse line. Does it need a trailing backslash?\n";
		print "$.:$1 is a duplicate or does not exist in English\n" and next unless exists $tr{$1};
		print "$.:Format for $1 does not match English format\n" unless $tr{$1} eq getfmt($2);
		delete $tr{$1};
	}
	print "$_ is not translated\n" for keys %tr;
	close FILE;
}

undef %en;

##########################################################################

print "\nChecking help strings.\nLoading english.\n";

open FILE, "< lang/help_english.h", or die "Could not open lang/help_english.h.\n";

while(<FILE>){
	print "$.:Tab characters are forbidden.\n" if /\t|\\t/;
	next unless (/START_HELP_TEXT/ .. /END_HELP_TEXT/) && /\s--(.*?)[[ =\\]/;
	$en{$1}=$_;
}

close FILE;


while(<lang/help*>){
	next if /english/;
	open FILE, "< $_" or die "Could not open $_.\n";
	/_(.*)\./;
	print "Checking $1.\n";
	my %tr=%en;
	while(<FILE>){
		print "$.:Tab characters are forbidden.\n" if /\t|\\t/;
		next unless (/START_HELP_TEXT/ .. /END_HELP_TEXT/) && /\s--(.*?)[[ =\\]/;
		print "$.:Switch --$1 is a duplicate or does not exist in English.\n" unless exists $tr{$1};
		delete $tr{$1};
	}
	print "Switch --$_ is not translated.\n" for keys %tr;
	close FILE;
}
 