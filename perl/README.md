# `find-diary.pl`

This script has a long history, and was essentially designed to keep track of files edited each day on various devices as part of the way I adhered to requirements needed to fulfil [ISO 9001](https://en.wikipedia.org/wiki/ISO_9000) at my then employer. I have used it for this purpose ever since, despite changes in employer, the way in which ISO 9001 is implemented, and changes to the standard itself. I suspect it is now not at all useful for ISO 9001 compliance (`git` and version history in various Cloud utilities now being far superior). However, I have on occasion found it useful for reconstructing what I was doing on one day or another, and still run it.

The script is written in Perl. To use it, you need to install it, and then do some setting up so that it is scheduled to run on your system. In the good old days, on Unix systems, this was done with `cron`, but on Windows and Macs, everything is more complicated of course. The general approach is to create a script called `Diary.sh` containing your execution of `find-diary.pl` and then have your scheduler call that script.

The version of `find-diary.pl` here is now the "official" one I propose to keep up-to-date. (I am probably now the only user of it in my organization.) It contains a few modifications from earlier versions, of the form `find-diary${X}.pl`, where `$X` is an integer (most recently `4`). For reference, the main change is using [markdown](https://en.wikipedia.org/wiki/Markdown) rather than a bespoke text format, along with a few other bits of code tidying and minor additional information added to reports.

## IMPORTANT WARNING

With the increasing use of cloud-based utilities to provide file storage, if you have not configured cloud-based files (e.g. Dropbox, Google, OneDrive, Box, iCloud, ...) to 'mirror' your cloud storage locally on your hard drive, `find-diary.pl` will not pick up that files have been modified.

Even if you have configured to 'mirror' (which you might reasonably not have done to save disk space), there is then a reliance on the cloud utility's app to do the mirroring reliably; experience has shown that this is not something that can be depended upon. Properly, the script should use each cloud utility's API to check, but this requires a lot of mucking around with API keys, certificates and authentication -- for each service provider's idiosyncratic ways of handling them -- that I currently feel disinclined to engage with.

** `find-diary.pl` focuses on files that are modified on your computer's storage **

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
  + `-prompt` -- Print a message to stdout before terminating -- this was largely provided so that `cron` would email the user the output as a 'prompt' to annotate the diary file with any information about what was done; this practice not having been kept up (especially when huge numbers of files are created), and the days of operating systems emailing users largely having been superceded, it's probably not useful any more
  + `-log $LOG_FILE` -- Record a log message to `$LOG_FILE`
  + `-owner` -- Include files not owned by the real or effective user ID 

`$SEARCH_DIRS` is a list of full paths you want searched for files that have been modified in the last day.

## Setting up the scheduler

### Creating your standard `find-diary.pl` call

It is a good idea to set up the `find-diary.pl` call you want to do in a file called `Diary.sh`, which you can then edit to adjust preferences and other matters.

An example, which assumes you have a directory called `Diary` in your home directory, into which you have a copy of `find-diary.pl` and where you will save this as `Diary.sh`:

```sh
#!/bin/sh
date >> "$HOME/Diary/diary.log"
nice "$HOME/Diary/find-diary.pl" -diary "$HOME/Diary" -personal -temp -log "$HOME/Diary/diary-log-`hostname`.txt" "$HOME/Documents" "$HOME/Dropbox" "$HOME/Google Drive"
```

Note the use of `nice` to give the script low priority. This is advised, especially when scheduling to run the script during the working day when your computer is likely to be on.

### Scheduling -- Linux

The `cron` documentation tells you how to schedule tasks to run regularly on Linux environments. Some Linux machines are, however, configured to disable users from running `cron` jobs. If that is not the case, then you should be able to use the following command to edit your so-called "`cron` table": `crontab -e`. (If you want to see what your `cron` table is, do `crontab -l`.) You may want to set your `$EDITOR` environment variable to your preferred terminal editor first -- e.g. `EDITOR=vi crontab -e`. When you edit the `cron` table, helpful linux implementations use comments (lines beginning `#`) to tell you the format, but essentially, you need one line to ensure the diary command is run, with the following format:

`$mm $hh * * * $HOME/Diary/Diary.sh`

Where `$hh:$mm` is the time of day (every day) that you want to run the `Diary.sh` script (assumed to be in the Diary subfolder of your home directory in the above -- you are advised to replace `$HOME` in the above with whatever is returned by `echo $HOME`). If you wanted to run it only to cover working days, you might enter:

`17 06 * * 2-6 /home/me/Diary/Diary.sh`

This will run `Diary.sh` from the directory `/home/me/Diary` on Tuesday to Saturday at 06:17 (in the morning). Note that your computer needs to be switched on at the time, otherwise the job may not run (this depends on operating system and computer configuration, however).

### Scheduling -- Mac

Although Macs have an installation of `cron`, the engineers at Apple have evidently decided it needs 'improvement', and have implemented a toolsuite around `launchd`. In a break with the "do one thing, do it well" philosophy of Unix operating system software design, `launchd` does a lot more than just schedule commands to run at particlar times. The text below will focus on configuring `launchd` to schedule your `Diary.sh` script to run, from what I can gather from the [documentation](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/Introduction.html#//apple_ref/doc/uid/10000172i-SW1-SW1).

In fact, the process is quite simple. You essentially create a "property list" file (which is an XML file with a `.plist` suffix) and, once you are happy with it, save it to `$HOME/Library/LaunchAgents`. Pretty much the instant you've put the file there, you will get a notification acknowledging its addition to the set of things your computer's `launchd` is going to take care of.

An example file, which I've named `uk.ac.hutton.Diary.plist` (following the seeming convention in other files in `$HOME/Library/LaunchAgents` that the file should be called `${label}.plist`, where `$label` is the entry in the `<Label>` tag), is given below:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>uk.ac.hutton.Diary</string>
    <key>ProgramArguments</key>
    <array>
        <string>/Users/me/Diary/Diary.sh</string>
    </array>
    <key>StartCalendarInterval</key>
    <dict>
        <key>Minute</key>
        <integer>03</integer>
        <key>Hour</key>
        <integer>12</integer>
    </dict>
</dict>
</plist>
```

This schedules `Diary.sh` in `/Users/me/Diary` to run every day at three minutes past noon. Note that [the documentation for `launchd`](https://developer.apple.com/library/archive/documentation/MacOSX/Conceptual/BPSystemStartup/Chapters/CreatingLaunchdJobs.html#//apple_ref/doc/uid/10000172i-SW7-BCIEDDBJ) seems to suggest that it doesn't trust things that run for less than 10 seconds, and will disable them (see the box marked "Important"). It may therefore be prudent to add `sleep 11` to the end of your `Diary.sh` invocation script.

### Scheduling -- Windows

With Perl being largely a unix-based language, using the script on Windows machines has always been something of a nuisance, and is sensitive to your computing and administrative environment. To run Perl on Windows, you will almost certainly need to install some software, which may contravene your organization's IT policy. At the time of writing, there is documentation at [https://www.perl.org/get.html#win32](https://www.perl.org/get.html#win32) on installation options for Windows that directly involve installing Perl. One alternative option is to install [Cygwin](https://www.cygwin.com/), and get Perl via that process, though with this only supporting up to [Windows 8](https://en.wikipedia.org/wiki/Windows_8), it's unlikely it's going to work on any modern personal computer. (The final release was at the end of 2016 according to Wikipedia, though [Windows 8.1](https://en.wikipedia.org/wiki/Windows_8.1) had a release in early 2023 apparently.)

The following assumes you are able to install one of the [Windows Subsystem for Linux](https://learn.microsoft.com/en-us/windows/wsl/install), which will then get you Perl. However, I haven't done this for several years, and the instructions below may be superceded. The approach uses [Windows Task Scheduler](https://learn.microsoft.com/en-us/windows/win32/taskschd/task-scheduler-start-page) to run a [batch file](https://en.wikipedia.org/wiki/Batch_file) that calls the `Diary.sh` invocation shell script for `find-diary.pl`. 

Essentially, you will put the Perl script `find-diary.pl` and your `Diary.sh` script in a folder like `C:\Users\me\Diary`, which from the linux subsystem will be named `/mnt/c/Users/me/Diary`. The example `Diary.sh` above would look something like this (replacing `/mnt/c/Users/me` with your Windows home directory path -- note that your linux subsystem home directory path may be different).

```sh
#!/bin/sh
date >> "/mnt/c/Users/me/Diary/diary.log"
nice "/mnt/c/Users/me/Diary/find-diary.pl" -diary "/mnt/c/Users/me/Diary" -personal -temp -log "/mnt/c/Users/me/Diary/diary-log-`hostname`.txt" "/mnt/c/Users/me/Documents" "/mnt/c/Users/me/Dropbox" "/mnt/c/Users/me/Google Drive"
```

You then create a Windows batch script (called `Diary.bat`) to run the script that looks like this:

```bat
bash -c "/mnt/c/Users/me/Diary/diary.sh"
```

You then use Windows' GUI tools to schedule a 'task'. Search for 'Task Scheduler', and then follow the instructions on the wizard:

  + Give the task a name and description as you see fit.
  + Choose to run it Daily from the radio buttons.
  + Give it a time of day to run (e.g. 12:37:19).
  + Select "Start a Program" as the action.
  + Use Browse... to find `Diary.bat` or enter the path to it manually
  + Click Finish

You can confirm the task has been scheduled using the 'Task Scheduler Library' app.