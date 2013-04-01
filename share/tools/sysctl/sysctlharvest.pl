#!/usr/bin/perl -Tw
#-
# Copyright (c) 2012 Dag-Erling Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

require v5.10;
use strict;
use warnings;
use utf8;
use Getopt::Long;
use Pod::Usage;
use constant SYSCTL_CMD => "/sbin/sysctl";
use constant XML_PREAMBLE => "<?xml version='1.0' encoding='utf-8'?>\n";

# Command-line options
our $from_sysctl;
our $from_binaries;
our $from_source;
our $source_path;
our $tunables;
our $output;
our $clobber;

# Hashes of harvested sysctls and tunables
our %sysctl;
our %tunable;

# Set of leaves
our %leaves;

#
# Harvest from the output of the sysctl(1) command
#
sub harvest_from_sysctl() {
    open(my $ph, "-|", SYSCTL_CMD, "-Ade")
	or die("unable to execute @{[SYSCTL_CMD]}\n");
    while (<$ph>) {
	next unless m/^([\w%-]+(?:\.[\w%-]+)*)=(.*?)\s*$/;
	my ($name, $short) = ($1, $2);

	if ($name =~ m/^dev\.(\w+)\.\d+\.([\w%-]+(?:\.[\w%-]+)*)$/) {
	    # per-device
	    my ($device, $variable) = ($1, $2);
	    # skip the device metadata
	    next if $variable =~ m/^%/;
	    $name = "dev.$device.N.$variable";
	} elsif ($name =~ m/^dev\.(\w+)\.([\w%-]+(?:\.[\w%-]+)*)$/) {
	    # per-driver
	    my ($driver, $variable) = ($1, $2);
	    # skip the device metadata
	    next if $variable =~ m/^%/;
	    $name = "dev.$driver.$variable";
	}

	my @path = split(/\./, $name);
	$leaves{$name} = [ @path ];
	my $node = \%sysctl;
	foreach my $cnp (@path) {
	    $node->{$cnp} //= { "-name" => $cnp };
	    $node = $node->{$cnp};
	}
	$node->{"-leaf"} = 1;
	$node->{"-short"} ||= $short;
    }
    close($ph);
}

# Quote XML-unsafe characters
sub quote($) { $_[0] =~ s/([^\s\w.-])/&#@{[ord $1]};/gr; }

#
# Output an XML fragment describing a particular sysctl node and all
# its descendents.  The first argument is the file handle to write to;
# the second is a hashref to the node; the third is the indentation
# level; any subsequent arguments, if present, are interpreted as a
# path to a particular subtree (or leaf).
#
sub output_tree($$$;@);
sub output_tree($$$;@) {
    my ($fh, $tree, $level, @path) = @_;

    print($fh XML_PREAMBLE,
	  "<sysctl:tree xmlns:sysctl='http://www.FreeBSD.org/XML/sysctl'>\n")
	if $level == 1;
    my $indent = "  " x $level;
    foreach (@path ? shift @path : sort keys %$tree) {
	next if m/^-/;
	my $node = $$tree{$_};
	print($fh "$indent<sysctl:node");
	foreach my $attr (qw/name type readonly/) {
	    print($fh " $attr='", quote($$node{"-$attr"}), "'")
		if $$node{"-$attr"};
	}
	print($fh ">\n");
	print($fh "$indent  <sysctl:short>",
	      quote($$node{"-short"} || "No description available"),
	      "</sysctl:short>\n")
	    if $$node{"-leaf"};
	output_tree($fh, $node, $level + 1, @path);
	print($fh "$indent</sysctl:node>\n");
    }
    print($fh "</sysctl:tree>\n")
	if $level == 1;
}

#
# Top-level output routine
#
sub output() {
    if (defined($output) && -d $output) {	
	foreach my $leaf (sort keys %leaves) {
	    my $fn = "$output/$leaf.xml";
	    next if (!$clobber && -f $fn);
	    open(my $fh, ">", $fn)
		or die("$fn: $!\n");
	    output_tree($fh, \%sysctl, 1, @{$leaves{$leaf}});
	    close($fh);
	}
    } else {
	$output //= "/dev/stdout";
	open(my $fh, ">", $output)
	    or die("$output: $!\n");
	output_tree($fh, \%sysctl, 1);
	close($fh);
    }
}

MAIN:{
    $ENV{PATH} = "/bin:/sbin:/usr/bin:/usr/sbin";
    $ENV{PERLDOC} = "-wcenter:'FreeBSD documentation tools'";
    GetOptions(
	"sysctl+"	=> \$from_sysctl,
	"binaries+"	=> \$from_binaries,
	"source+"	=> \$from_source,
	"sys=s"		=> \$source_path,
	"tunables+"	=> \$tunables,
	"output=s"	=> \$output,
	"clobber+"	=> \$clobber,
	"help"		=> sub { pod2usage(-exitval => 0, -verbose => 0); },
	"man"		=> sub { pod2usage(-exitval => 0, -verbose => 2); })
	or pod2usage(1);

    if (!$from_sysctl && !$from_binaries && !$from_source) {
	$from_sysctl = 1;
    }

    harvest_from_sysctl()
	if $from_sysctl;
#    harvest_from_binaries()
#	if $from_binaries;
#    harvest_from_source()
#	if $from_source;

    # output the result
    # XXX allow the user to specify which nodes / subtrees to output
    # XXX through @ARGS
    output();
}

=pod

=encoding utf8

=head1 NAME

sysctlharvest - Generate sysctl documentation stubs

=head1 SYNOPSIS

sysctlharvest [options]

 Options:
   --help           brief help message
   --man	    complete documentation
   --sysctl         use sysctl tree
   --binaries       use kernel binaries
   --source         use kernel source
   --sys=path       path to kernel source
   --tunables       include tunables
   --output=path    output file or directory
   --clobber        overwrite existing files

=head1 OPTIONS

=over 8

=item B<--help>

Print a brief help message and exits.

=item B<--man>

Print the complete documentation and exits.

=item B<--sysctl>

Harvest information from the sysctl tree of the current system.

=item B<--source>

Harvest information from a kernel source tree.

=item B<--sys>=I<path>

Used with the B<--source> option to specify the location of the kernel
source tree.  The default is to look first in the current directory,
then in a C<sys> subdirectory, then in C</sys>, and finally in
C</usr/src/sys>.

=item B<--tunables>

When harvesting from a kernel source tree, include tunables.

=item B<--output>=I<path>

Output path.  If the path exists and is a directory, individual files
are created for each sysctl or tunable.  If it does not exist, or it
exists and is a file, all the harvested information is written to a
single file.  The default is to output all the information to stdout.

=item B<--clobber>

When writing individual files, overwrite any that already exist.

=back

=head1 DESCRIPTION

The B<sysctlharvest> utility gathers information about sysctl nodes
and generates documentation stubs based on that information.

=head1 HISTORY

The B<sysctlharvest> utility and this manual page were written by
Dag-Erling Smørgrav <des@freebsd.org>.

=cut

1;
