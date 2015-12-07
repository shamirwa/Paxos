#!/usr/bin/perl
#
#
if($#ARGV + 1 < 3){
    print "usage: ./copyAndRun.pl <sourcePath> <startVM> <endVM> <passwd>\n";
    print $ARGV[0];
    print "\n" . $ARGV[1];
    print "\n" . $ARGV[2];
    exit;
}

my $sourceLoc = $ARGV[0];
my $startVM = $ARGV[1];
my $endVM = $ARGV[2];
my $lieus = "lieus.txt";
my $evalTime = 30 ; # seconds
my $cmd = "";
my $projDir = "/root/myproj3/";


if($#ARGV > 3){
    $passwd = $ARGV[3];
}


for($vm = $startVM; $vm <= $endVM; $vm++){

    # Command to copy the files
    $cmd = "scp -r " . $sourceLoc . " root\@10.0.1." . $vm . ":/root/";
    system($cmd);

}

# Make the source code
$cmd = "pssh -p 4 -P -h $lieus -t $evalTime \"cd $projDir; make clean; make\" ";
system($cmd);

print "All files are copied successfully\n";

