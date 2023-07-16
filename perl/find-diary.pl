#!/usr/bin/perl
#
# find-diary.pl
#
# Searches through homespace finding files created or modified on a specified
# date, and can optionally continue doing the same from the specified date
# right up to the present.
#
# usage: find-diary [-topresent] <date as YYYYMMDD> [dirs...]

use strict;
use Time::Local;
use Fcntl qw/:flock/;
use Sys::Hostname;

# Globals

my $topresentflag = 0;
my $verbose = 0;
my $access = 1;
my $personal = 1;
my $temp = 1;
my $hidden = 1;
my $log = 0;
my $logfile;
my $lastlogdate;
my $diaryflag = 0;
my $diary;
my $prompt = 0;
my $owner = 0;
my $promptmsg;
my $startdate;
my $currentdate;
my $currenttime;
my $startdaysago;
my $enddaysago;
my $enddate;
my $secsperday = 24 * 60 * 60;
my $tries = 30;
my $hostname = hostname();
my @dirs;
my %ignore;

my %os_to_path = ( 'cygwin' => '/',
		   'solaris' => '/',
		   'MSWin32' => '/' );

my $psep = defined($os_to_path{$^O}) ? $os_to_path{$^O} : '/';

# Parse command line

while(substr($ARGV[0], 0, 1) eq '-') {
  my $option = shift(@ARGV);
  if($option eq '-topresent') {
    $topresentflag = 1;
  }
  elsif($option eq '-verbose') {
    $verbose = 1;
  }
  elsif($option eq '-access') {
    $access = 0;
  }
  elsif($option eq '-personal') {
    $personal = 0;
  }
  elsif($option eq '-temp') {
    $temp = 0;
  }
  elsif($option eq '-hidden') {
    $hidden = 0;
  }
  elsif($option eq '-ignore') {
    $ignore{shift(@ARGV)} = 1;
  }
  elsif($option eq '-diary') {
    $diaryflag = 1;
    $diary = shift(@ARGV);
    if(!-d "$diary") {
      die "Diary directory $diary not found or not a directory\n";
    }
  }
  elsif($option eq '-prompt') {
    $prompt = 1;
  }
  elsif($option eq '-log') {
    $log = 1;
    $logfile = shift(@ARGV);
    $lastlogdate = "not found";
    if(open(LOG, "<$logfile")) {
      while(my $line = <LOG>) {
    	  if($line =~ /^(\d\d\d\d)(\d\d)(\d\d)\s/) {
	        $lastlogdate = "$1$2$3";
	      }
      }
      close(LOG);
    }
  }
  elsif($option eq '-owner') {
    $owner = 1;
  }
  else {
    die "Unrecognised option: $option\n";
  }
}

if($topresentflag && $diaryflag) {
  die "You can't have -topresent and -diary in the same search\n";
}

if($diaryflag) {
  $access = $access ? 0 : 1;
				# The meaning of the access flag is toggled
				# when the diaryflag is on
  $hidden = $hidden ? 0 : 1;
				# So is the meaning of the hidden flag.
}

# Get the date

if(!$log) {
  if($#ARGV < 0) {
    die "Usage: $0 [-access] [-personal] [-temp] [-topresent] [-verbose] "
      ."[-owner] [-hidden] [-diary] <date as YYYYMMDD> [dirs...]\n";
  }
  $startdate = shift(@ARGV);
  $startdate =~ s/\\//g;
}
else {
  if($lastlogdate ne "not found") {
    $startdate = &adddate($lastlogdate, 1);
  }
  else {
    $startdate = 'YESTERDAY';
  }
}

# Get the directories

while($#ARGV >= 0) {
  push(@dirs, shift(@ARGV));
}
if($#dirs < 0) {
  if(defined($ENV{'HOME'})) {
    push(@dirs, $ENV{'HOME'});
  }
  else {
    die "No directories specified, and no HOME environment variable\n";
  }
}

# Get today's date and time in seconds since midnight

my($sec, $min, $hr, $day, $mon, $yr, $wday) = localtime;

$currenttime = $sec + ($min * 60) + ($hr * 60 * 60);
$currentdate = sprintf("%04d%02d%02d", $yr + 1900, $mon + 1, $day);

# Compute the start date

if($startdate eq 'TODAY') {
  $startdate = $currentdate;
}
elsif($startdate eq 'YESTERDAY') {
  $startdate = &subdate($currentdate, 1);
}
elsif($startdate eq 'LASTWORKINGDAY') {
  my $lastworkingday = 1;	# Number of days ago to last working day

  if($wday == 1) {		# Monday today
    $lastworkingday = 3;	# Last working day is Friday
  }
  elsif($wday == 0) {		# Sunday today
    $lastworkingday = 2;	# Last working day is Friday
  }
  
  $startdate = &subdate($currentdate, $lastworkingday);
}
  

$startdaysago = &diffdate($currentdate, $startdate)
  + ($currenttime / $secsperday);
				# Should take us back to midnight on startdate

# Get the end date

if($topresentflag) {
  $enddaysago = 0;
  $enddate = $currentdate;
}
elsif($log) {
  $enddate = &subdate($currentdate, 1);
  $enddaysago = $currenttime / $secsperday;
  if($startdate > $enddate) {
    print "Diary is up to date\n";
    exit 0;
  }
}
else {
  $enddaysago = $startdaysago - 1;
  $enddate = $startdate;
  $enddaysago = 0 if $enddaysago < 0;
}

# Find the files

my %access;
my %modify;

foreach my $dir (@dirs) {
  &findfiles($dir, $startdaysago, $enddaysago, \%access, \%modify,
	     $currentdate, $currenttime);
}

my $login_name = getlogin();
if($login_name !~ /\S/) {
  $login_name = `whoami`;
  $login_name =~ s/^\s*//;
  $login_name =~ s/\s*$//;
  if($login_name !~ /\S/) {
    $login_name = 'unknown';
  }
}
my $user_name = $login_name;
#my $user_name = (split(/,/, (getpwuid($>))[5]))[0];
my $output;

my $diaryfile;

if($diaryflag) {
  # Here we need to gain exclusive access to the diary file to write the
  # data. The diary file could be shared between several people.
  # If so, we only want the date to be printed once, but will assume that
  # the startdate argument is the date to be appended to the file and that
  # if the startdate appears in the file already, then there is no other date
  # in between.

  my @diaryfromdate = &decomposedate($startdate);
  $diaryfile = "$diaryfromdate[0]-$diaryfromdate[1]-$hostname.md";
  my @diarytodate = &decomposedate($enddate);
  my $datedone = 0;
  my $opened = 0;

  $diaryfromdate[3] = &day($startdate);
  $diarytodate[3] = &day($enddate);
  my $dateline = "# $diaryfromdate[0]/$diaryfromdate[1]/$diaryfromdate[2]".
    " $diaryfromdate[3]";
  if($startdate ne $enddate) {
    $dateline .= " to $diarytodate[0]/$diarytodate[1]/$diarytodate[2]".
      " $diarytodate[3]";
  }

  umask(2);			# Allow group write on the diary
  do {
    if(open(DIARY, "+>>$diary$psep$diaryfile")) {
				# Read and append. Experimentation has shown
				# that writes are only successful once all the
				# file has been read in.
      for(my $i = 1; $i <= $tries; $i++) {
	      if(flock(DIARY, LOCK_EX | LOCK_NB)) {
				  # Try to get a lock without waiting
          $output = *DIARY;
	        $opened = 1;
          $promptmsg = "$diary$psep$diaryfile";
          last;
        }
        else {
	        sleep(int(rand() * 100) + 10);
        }
      }
      if($opened) {
        while(my $line = <DIARY>) {
          chomp $line;

          if($line eq $dateline) {
            $datedone = 1;
          }
        }
      }
      else {
				# No lock obtainable. Write to stdout.
        close(DIARY);
        $output = *STDOUT;
        $opened = 1;

        print "Could not gain an exclusive lock on diary file "
          ."$diary$psep$diaryfile after $tries attempts\n";
      }
    }
    elsif(!-e "$diary$psep$diaryfile") {
				# Maybe the file didn't exist. Try creating it
				# and grabbing a lock.
      open(DIARY, ">$diary$psep$diaryfile")
        || die "Cannot create diary file $diary$psep$diaryfile: $!\n";
      if(!flock(DIARY, LOCK_EX | LOCK_NB)) {
        close(DIARY);
        sleep(int(rand() * 100) + 10);
        next;
				# Next time round the loop the file will be
				# opened for appending.
      }
      $output = *DIARY;
      $promptmsg = "$diary$psep$diaryfile";
      $opened = 1;
    }
    else {
      $output = *STDOUT;
      $opened = 1;

      print "Could not write to diary file $diary$psep$diaryfile: $!\n";
    }
  } while(!$opened);

  print $output "\n$dateline\n" if !$datedone;
}
else {
  $output = *STDOUT;
}

my $subtitle = 0;

for(my $date = $startdate; $date <= $enddate; $date = &incdate($date)) {
  my $showdate = 0;
  if(!$diaryflag) {
    print $output "$date:\n";
  }
  if(($access && defined($access{$date})) || defined($modify{$date})) {
    if($diaryflag
       && (defined($modify{$date}) || ($access && defined($access{$date})))) {
      if(!$subtitle) {
	      print $output "## $user_name\n";
	      $subtitle = 1;
	      if(defined($promptmsg)) {
	        if($startdate eq $enddate) {
	          $promptmsg = "Diary entry made for $startdate in file $promptmsg";
	        }
	        else {
	          $promptmsg = "Diary entry made for $startdate to $enddate in file".
	          " $promptmsg";
	        }
	      }
      }
      else {
	      print $output "\n";
      }
      if($startdate ne $enddate) {
	      $showdate = 1;
      }
    }
    if($access) {
      if($showdate) {
	      print $output "### Last accessed $date:\n";
      }
      else {
	      print $output "### Accessed:\n";
      }
      foreach my $accessed (@{$access{$date}}) {
        print $output "  + `$hostname:$accessed`\n";
      }
    }
    if($showdate) {
      print $output "### Last modified $date: \n";
    }
    else {
      print $output "### Modified:\n" unless $diaryflag && !$access;
    }
    foreach my $modified (@{$modify{$date}}) {
      print $output "  + `$hostname:$modified`\n";
    }
  }
  else {
    print $output "Nothing\n" unless $diaryflag;
    undef $promptmsg if $diaryflag;
  }
}

if($log) {
  if(-e "$logfile") {
    open(LOG, ">>$logfile") or die "Cannot append to log file $logfile: $!\n";
  }
  else {
    open(LOG, ">$logfile") or die "Cannot create log file $logfile: $!\n";
  }
  if($diaryflag) {
    print LOG "$enddate ($startdate) by $login_name on ".
      "$hostname in $diaryfile: ", join(" ", @dirs), "\n";
  }
  else {
    print LOG "Non-diary search $enddate ($startdate) by $login_name on ".
      "$hostname: ", join(" ", @dirs), "\n";
  }
  close(LOG);
}

if($prompt && defined($promptmsg)) {
  print "$promptmsg\n";
}

exit 0;

sub findfiles {
  my ($dir, $start, $end, $access, $modify, $date, $time) = @_;

  if(!-d "$dir") {
    warn "$dir is not a directory!\n";
    return;
  }
  elsif($verbose) {
    print "Searching $dir...\n";
#    print "< $start, >$end from $date + $time seconds\n";
  }
  opendir(DIR, "$dir");
  my @files = grep(!/^\.\.?$/, readdir(DIR));
  closedir(DIR);

  foreach my $file (@files) {
    my $fullfile = "$dir$psep$file";

    next if -l "$fullfile";
    if(!$hidden) {
      next if $file =~ /^\./;
    }
    if(-d "$fullfile") {
      next if !$personal && ($file =~ /^personal/i || $file =~ /^private/i);
      next if defined($ignore{$fullfile});
      next if $diaryflag && $fullfile eq $diary;
      &findfiles($fullfile, $start, $end, $access, $modify, $date, $time);
    }
    else {
      next if !-e "$fullfile";
      next if !$owner && !-o "$fullfile" && !-O "$fullfile";
      if(!$temp) {
	      next if $file =~ /^~\$/;
        next if $file =~ /~$/;
        next if $file =~ /^#.*#$/;
        next if $file =~ /\.tmp$/i;
	      next if $file =~ /\.wbk$/i;
	      next if $file =~ /^Thumbs.db$/i;
        next if $file eq "Icon";
      }
      my $mdays = -M "$fullfile";
      my $adays = -A "$fullfile";

      if($mdays < $start && $mdays > $end) {
	      my $mdate = &subdate($date, $time, $mdays);
	      push(@{$$modify{$mdate}}, $fullfile);
      }
      if($adays < $start && $adays > $end) {
	      my $adate = &subdate($date, $time, $adays);
	      push(@{$$access{$adate}}, $fullfile);
      }
    }
  }
}

sub subdate {
  my ($date, $time, $nsubdays) = @_;

  if(!defined($nsubdays)) {	# Two argument call
    $nsubdays = $time;
    $time = 12 * 3600;		# Set the time to noon on the date passed in
				# to ensure that subtracting n day's worth of
				# seconds will put us into the right day
				# (allowing for DST, leap seconds, etc.)
  }

  my($yr, $mon, $day) = &decomposedate($date);
  my $datesec = timelocal(0, 0, 0, $day, $mon - 1, $yr - 1900) + $time;
  my $subdatesec = $datesec - ($nsubdays * $secsperday);
  my($newsec, $newmin, $newhr, $newday, $newmon, $newyr)
    = localtime($subdatesec);
  return sprintf("%04d%02d%02d", $newyr + 1900, $newmon + 1, $newday);
}

sub adddate {
  my ($date, $time, $nadddays) = @_;

  if(!defined($nadddays)) {
    $nadddays = $time;
    $time = 12 * 3600;
  }

  return &subdate($date, $time, -$nadddays);
}

sub day {
  my ($date) = @_;

  my($yr, $mon, $day) = &decomposedate($date);
  my $datesec = timelocal(0, 0, 12, $day, $mon - 1, $yr - 1900);

  return ("Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat")[(localtime($datesec))[6]];
}


sub decomposedate {
  my ($date) = @_;

  if($date =~ /^(\d{4})(\d{2})(\d{2})$/) {
    return ($1, $2, $3);
  }
  else {
    die "Invalid date: $date\n";
  }
}

sub diffdate {
  my ($lhdate, $rhdate) = @_;

  if($lhdate eq $rhdate) {
    return 0;
  }

  my($lhyr, $lhmon, $lhday) = &decomposedate($lhdate);
  my($rhyr, $rhmon, $rhday) = &decomposedate($rhdate);

  my $diffsec = timelocal(0, 0, 5, $lhday, $lhmon - 1, $lhyr - 1900)
    - timelocal(0, 0, 5, $rhday, $rhmon - 1, $rhyr - 1900);
  my $diffdy = int($diffsec / $secsperday);
  if($diffsec - ($diffdy * $secsperday) >= 0.5) {
    $diffdy++;
  }

  return $diffdy;
}

sub incdate {
  my ($date) = @_;

  my($yr, $mon, $day) = &decomposedate($date);

  my $datesec = timelocal(0, 0, 5, $day, $mon - 1, $yr - 1900);

  $datesec += $secsperday;
  
  my($newsec, $newmin, $newhr, $newday, $newmon, $newyr) = localtime($datesec);
  return sprintf("%04d%02d%02d", $newyr + 1900, $newmon + 1, $newday);
}
