package Dahdi::Xpp;
#
# Written by Oron Peled <oron@actcom.co.il>
# Copyright (C) 2007, Xorcom
# This program is free software; you can redistribute and/or
# modify it under the same terms as Perl itself.
#
# $Id$
#
use strict;
use Dahdi::Xpp::Xbus;

=head1 NAME

Dahdi::Xpp - Perl interface to the Xorcom Astribank drivers.

=head1 SYNOPSIS

  # Listing all Astribanks:
  use Dahdi::Xpp;
  # scans hardware:
  my @xbuses = Dahdi::Xpp::xbuses("SORT_CONNECTOR");
  for my $xbus (@xbuses) {
    print $xbus->name." (".$xbus->label .", ". $xbus->connector .")\n";
    for my $xpd ($xbus->xpds) {
      print " - ".$xpd->fqn,"\n";
    }
  }

=cut


my $proc_base = "/proc/xpp";

# Nominal sorters for xbuses
sub by_name {
	return $a->name cmp $b->name;
}

sub by_connector {
	return $a->connector cmp $b->connector;
}

sub by_label {
	my $cmp = $a->label cmp $b->label;
	return $cmp if $cmp != 0;
	return $a->connector cmp $b->connector;
}

sub score_type {
	my $score;

	return 1 if grep(/\b[ETJ]1/, @_);
	return 2 if grep(/\bBRI/, @_);
	return 3 if grep(/\bFXO/, @_);
	return 4;	# FXS
}

sub by_type {
	my @a_types = map { $_->type } $a->xpds();
	my @b_types = map { $_->type } $b->xpds();
	my $res;

	my $a_score = score_type(@a_types);
	my $b_score = score_type(@b_types);
	#printf STDERR "DEBUG-a: %s %s %s\n", $a->name, $a_score, join(',',@a_types);
	#printf STDERR "DEBUG-b: %s %s %s\n", $b->name, $b_score, join(',',@b_types);
	$res = $a_score <=> $b_score;
	$res = $a->connector cmp $b->connector if $res == 0;
	return $res;
}


=head1 xbuses([sort_order])

Scans system (/proc and /sys) and returns a list of Astribank (Xbus) 
objects. The optional parameter sort_order is the order in which 
the Astribanks will be returns:


=head1 sorters([sort_order])

With no parameters, returns the names of built in sorters.
With a single parameter, returns a reference to the requested built in sorter.
Also, for convenience, a reference to a custom sorter function may be passed
and returned as is.

The built in sorters are:

=over

=item SORT_CONNECTOR

Sort by the connector string. For USB this defines the "path" to get to
the device through controllers, hubs etc.

=item SORT_LABEL

Sorts by the label of the Astribank. The label field is unique to the
Astribank. It can also be viewed through 'lsusb -v' without the drivers
loaded (the iSerial field in the Device Descriptor). This is normally
relieble, but some older Astribanks have an empty label.

=item SORT_NAME

Sort by the "name". e.g: "XBUS-00". The order of Astribank names depends
on the load order, and hence may change between different runs.

=item SORT_TYPE

Sort by XPD types. First Astribanks with E1/T1/J1 XPDs, then with BRI,
then with FXO, then ones with only FXS ports. Within each type they
are sorted by the connector field (as in SORT_CONNECTOR above).

=item custom function

Instead of using a predefined sorter, you can pass your own sorting
function. See the example sorters in the code of this module.

=back

=cut

sub sorters {
	my %sorter_table = (
		SORT_CONNECTOR	=> \&by_connector,
		SORT_NAME	=> \&by_name,
		SORT_LABEL	=> \&by_label,
		SORT_TYPE	=> \&by_type,
		# Aliases
		connector	=> \&by_connector,
		name		=> \&by_name,
		label		=> \&by_label,
		type		=> \&by_type,
	);
	my $which_sorter = shift || return sort keys %sorter_table;
	return $which_sorter if ref($which_sorter) eq 'CODE';
	return $sorter_table{$which_sorter};
}

sub xbuses {
	my $optsort = shift || 'SORT_CONNECTOR';
	my @xbuses;

	-d "$proc_base" or return ();
	my @lines;
	local $/ = "\n";
	open(F, "$proc_base/xbuses") ||
		die "$0: Failed to open $proc_base/xbuses: $!\n";
	@lines = <F>;
	close F;
	foreach my $line (@lines) {
		chomp $line;
		my ($name, @attr) = split(/\s+/, $line);
		$name =~ s/://;
		$name =~ /XBUS-(\d\d)/ or die "Bad XBUS number: $name";
		my $num = $1;
		@attr = map { split(/=/); } @attr;
		my $xbus = Dahdi::Xpp::Xbus->new(NAME => $name, NUM => $num, @attr);
		push(@xbuses, $xbus);
	}
	my $sorter = sorters($optsort);
	die "Unknown optional sorter '$optsort'" unless defined $sorter;
	@xbuses = sort $sorter @xbuses;
	return @xbuses;
}

sub xpd_of_span($) {
	my $span = shift or die "Missing span parameter";
	return undef unless defined $span;
	foreach my $xbus (Dahdi::Xpp::xbuses('SORT_CONNECTOR')) {
		foreach my $xpd ($xbus->xpds()) {
			return $xpd if $xpd->fqn eq $span->name;
		}
	}
	return undef;
}

=head1 sync([new_sync_source])

Gets (and optionally sets) the internal Astribanks synchronization
source. When used to set sync source, returns the original sync source.

A synchronization source is a value valid writing into /proc/xpp/sync .
For more information read that file and see README.Astribank .

=cut

sub sync {
	my $newsync = shift;
	my $result;
	my $newapi = 0;

	my $file = "$proc_base/sync";
	return '' unless -f $file;
	# First query
	open(F, "$file") or die "Failed to open $file for reading: $!";
	while(<F>) {
		chomp;
		/SYNC=/ and $newapi = 1;
		s/#.*//;
		if(/\S/) {	# First non-comment line
			s/^SYNC=\D*// if $newapi;
			$result = $_;
			last;
		}
	}
	close F;
	if(defined($newsync)) {		# Now change
		$newsync =~ s/.*/\U$&/;
		if($newsync =~ /^(\d+)$/) {
			$newsync = ($newapi)? "SYNC=$1" : "$1 0";
		} elsif($newsync ne 'DAHDI') {
			die "Bad sync parameter '$newsync'";
		}
		open(F, ">$file") or die "Failed to open $file for writing: $!";
		print F $newsync;
		close(F) or die "Failed in closing $file: $!";
	}
	return $result;
}

=head1 SEE ALSO

=over

=item L<Dahdi::Xpp::Xbus>

Xbus (Astribank) object.

=item L<Dahdi::Xpp::Xpd>

XPD (the rough equivalent of a Dahdi span) object.

=item L<Dahdi::Xpp::Line>

Object for a line: an analog port or a time-slot in a adapter. 
Equivalent of a channel in Dahdi.

=item L<Dahdi>

General documentation in the master package.

=back

=cut

1;
