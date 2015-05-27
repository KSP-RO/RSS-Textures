#!/usr/bin/perl
use 5.010;
use strict;
use warnings;
use FindBin qw($Bin);
use File::Path qw(make_path);
use File::Copy qw(copy);
use File::Copy::Recursive qw(dircopy);
use File::Find qw(find);
use Archive::Zip qw(AZ_OK);
use autodie qw(:all dircopy copy);

# RSS-Textures build script.
# Paul '@pjf' Fenwick
# License: MIT

my @RESOLUTIONS     = qw(2048 4096 8192);
my $BIOMES_PATTERN  = "Biomes/*.png";
my $BUILD_DIR       = "GameData";
my $BIOMES_DIR      = "$BUILD_DIR/RSS-Textures/PluginData";
my $TEXTURE_DIR     = "$BUILD_DIR/RSS-Textures";
my $DEBUG           = 1;

chdir "$Bin/.." ;

-f $BUILD_DIR and die "$BUILD_DIR already exists, please (re)move it before running.\n";

make_path $BIOMES_DIR;

# Copy Biomes

say "Copying biomes to $BIOMES_DIR" if $DEBUG;
foreach my $file (glob($BIOMES_PATTERN)) {
    copy($file, $BIOMES_DIR);
}

# Copy each set of textures and zip
foreach my $resolution (@RESOLUTIONS) {
    say "Copying $resolution textures" if $DEBUG;
    dircopy($resolution, $TEXTURE_DIR);

    say "Building $resolution.zip" if $DEBUG;
    my $zip = Archive::Zip->new;

    my @files;

    # Find all the files we need to add.

    find(sub {
        return if not -f; # Only add files (not dirs) to our zip
        push @files, $File::Find::name;
    }, $BUILD_DIR);

    # Add them with path names intact.

    foreach my $file (@files) {
        $zip->addFile($file);
    }

    # Hooray, we're done!

    $zip->writeToFileNamed("$resolution.zip") == AZ_OK
        or die "Failed to write $resolution.zip";
}

say "Done!" if $DEBUG;
