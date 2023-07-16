# `find-diary.pl`

This script has a long history, and was essentially designed to keep track of files edited each day on various devices as part of the way I adhered to requirements needed to fulfil ISO 9001 at my then employer. I have used it for this purpose ever since, despite changes in employer, the way in which ISO 9001 is implemented, and changes to the standard itself. I suspect it is now not at all useful for ISO 9001 compliance (`git` and version history in various Cloud utilities now being far superior). However, I have on occasion found it useful for reconstructing what I was doing on one day or another, and still run it.

The script is written in Perl. To use it, you need to install it, and then do some setting up so that it is scheduled to run on your system. In the good old days, on Unix systems, this was done with `cron`, but on Windows and Macs, everything is more complicated of course. The general approach is to create a script called `Diary.sh` containing your execution of `find-diary.pl` and then have your scheduler call that script.

## Command synopsis

The `find-diary.pl` script has two main modes, the obsolete `-topresent` not described here, and the `-diary ...` mode, which is.

To use it:

`./find-diary.pl -diary $DIARY_DIR [-access] [-personal] [-temp] [-hidden] {-ignore $IGNORE_DIR} [-prompt] [-log $LOG_FILE] [-owner] $SEARCH_DIRS`

where `$DIARY_DIR` is a directory you want to store the diary files in, `$IGNORE_DIR`s are directories in `$SEARCH_DIRS` you want to ignore, and `$LOG_FILE` is a file to save a log to. 

The options:

  + `-access` -- Include whether files were accessed (by default, just searches for modified)
  + `-personal` -- Ignore any file or directory with a name beginning `private` or `personal` (case insensitive)
  + `-temp` -- Ignore filenames beginning with `~$`, ending with `~`, beginning and ending with `#`, ending with `.tmp` or `.wbk` (case insensitive), named `Icon`, or named `Thumbs.db` (case insensitive)
  + `-hidden` -- Include filenames beginning with `.`
  + `-ignore $IGNORE_DIR` -- Ignore any directory encountered named `$IGNORE_DIR` (case sensitive); you can use this option more than once if there are several such directories you want ignored
  + `-prompt` -- Print a message to stdout before terminating (generally useful for debugging purposes)
  + `-log $LOG_FILE` -- Record a log message to `$LOG_FILE`
  + `-owner` -- Include files not owned by the real or effective user ID (will be necessary if using `launchd` on a Mac, which runs as root, or any other tool running as root not `sudo`ed)

`$SEARCH_DIRS` is a list of full paths you want searched for files that have been modified in the last day.

## Setting up the scheduler

### Creating your standard `find-diary.pl` call

It is a good idea to set up the `find-diary.pl` call you want to do in a file called `Diary.sh`, which you can then edit to adjust preferences and other matters.

An example, which assumes you have a directory called `Diary` in your home directory, into which you have a copy of `find-diary.pl`:

```sh
#!/bin/sh
date >> $HOME/Diary/diary.log
nice $HOME/Diary/find-diary.pl -diary "$HOME/Diary" -personal -temp -log "$HOME/Diary/diary-log-`hostname`.txt" $HOME/Documents $HOME/Dropbox "$HOME/Google Drive"
```

### Scheduling -- Mac

