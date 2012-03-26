# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

use strict;
use warnings;

package Clownfish::Hierarchy;
use Carp;

use Clownfish::Util qw( verify_args );
use Clownfish::Class;
use Clownfish::Parser;

our %new_PARAMS = (
    source => undef,
    dest   => undef,
);

sub new {
    my ( $either, %args ) = @_;
    verify_args( \%new_PARAMS, %args ) or confess $@;
    my $package = ref($either) || $either;
    my $parser = Clownfish::Parser->new;
    return $package->_new( @args{qw( source dest )}, $parser );
}

sub _do_parse_file {
    my ( $parser, $content, $source_class ) = @_;
    $content = $parser->strip_plain_comments($content);
    return $parser->file( $content, 0, source_class => $source_class, );
}

1;

__END__

__POD__

=head1 NAME

Clownfish::Hierarchy - A class hierarchy.

=head1 DESCRIPTION

A Clownfish::Hierarchy consists of all the classes defined in files within
a source directory and its subdirectories.

There may be more than one tree within the Hierarchy, since all "inert"
classes are root nodes, and since Clownfish does not officially define any
core classes itself from which all instantiable classes must descend.

=head1 METHODS

=head2 new

    my $hierarchy = Clownfish::Hierarchy->new(
        source => undef,    # required
        dest   => undef,    # required
    );

=over

=item * B<source> - The directory we begin reading files from.

=item * B<dest> - The directory where the autogenerated files will be written.

=back

=head2 build

    $hierarchy->build;

Parse every Clownfish header file which can be found under C<source>, building
up the object hierarchy.

=head2 ordered_classes

    my $classes = $hierarchy->ordered_classes;

Return all Classes as an array with the property that every parent class will
precede all of its children.

=head2 propagate_modified

    $hierarchy->propagate_modified($modified);

Visit all File objects in the hierarchy.  If a parent node is modified, mark
all of its children as modified.  

If the supplied argument is true, mark all Files as modified.

=head2 get_source get_dest

Accessors.

=cut