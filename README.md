

# Modified BSD */etc/rmt* as a captive shell

The intention is to use *rmt* as the shell for a captive account **tape** (say) which  remote backup applications such as *dump*, *xfsdump* and *gnu tar* could use **SSH** to dump to a remote tape or regular file. 

Access to file system locations such as devices, files and directories is restricted by a configuration file but not as extensive as that of *Schily tar rmt* implemetation.

Since some remote clients client such as *xfsdump* attempt to identify the remote system (Linux or Irix)  by trying *$RSH uname* which will be executed as *$SHELL -c uname* on the remote system this implementation has a builtin version of *uname*. 

In order to support *SSH* forced commands *rmt* retrieves *$SSH_ORIGINAL_COMMAND*  from the environment and honours that command.

## Examples

### Captive __tape__ user

````
#/etc/group
tape:x:10024:

#/etc/passwd
tape:x:10024:Captive remote tape access:/home/tape:/opt/rmt/sbin/rmt
````

### Access file

````
#/opt/rmt/etc/access
# Symbolic links are follow but both ends required
# eg /dev/nrmt0h --> /devices/tape/std0h
/dev/nrmt0h
/devices/tape/std0h
# 
/dev/nrst*
# File targets
/export/dumps/*
# For testing
/dev/null
````

### Captive account home directory etc

````
# These can be quite restrictive
drwx--x--- root tape /home/tape
drwx--x--- root tape /home/tape/.ssh
-r--r----- root tape /home/tape/.ssh/authorized_keys
````

### *Authorized_keys* file

````
# ~tape/.ssh/authorized_keys
# 
from="192.168.97.25",command="/opt/rmt/sbin/rmt",no-X11-forwarding,no-pty,no-agent-forwarding,no-port-forwarding ssh-rsa AAAAB3N.....
.....NUQ== backup@example.com
````

### Tape device permissions

````
# Tape devices or files must be readable for restores and writeable for dumps.
````

## Client side

### Basic test 

````bash
# Test with ssh will need to ensure dumphost is added to .ssh/known_hosts
# before testing this
IDENTITY=/etc/opt/backup/keys/backup_example
ssh -o BatchMode=yes -q -x -a -T -e none -i $IDENTITY tape@dumphost
O/dev/null
1
A0
W4
123
A4
C0
A0
^D
````

### Test extension

````bash
IDENTITY=/etc/opt/backup/keys/backup_example
ssh -o BatchMode=yes -q -x -a -T -e none -i $IDENTITY tape@dumphost uname
Linux
````

### Encapsulate the ssh command in a script to pass to *xfsdump* etc

````bash
#! /bin/sh
# @(#) /opt/backup/sbin/scmd
SSH=/usr/sbin/ssh
IDENTITY=/etc/opt/backup/keys/backup_example
exec $SSH -oBatchMode=yes -q -x -a -T -e none -i $IDENTITY $@
````

````bash
RSH=/opt/backup/sbin/scmd dump -0 -f tape@dumphost:/dev/null /boot
DUMP: Connection to dumphost established.
  DUMP: Date of this level 0 dump: Tue Oct 19 12:34:25 2021
  DUMP: Dumping /dev/sda2 (/ (dir boot)) to /dev/null on host tape@dumphost
  ....
````

Note that this **does not** work as *dump* does an *fork* and *exec* of "$RSH" 

```bash
RSH="/bin/ssh -o BatchMode=yes -q -x -a -T -e none -i /etc/opt/backup/keys/backup_example" dump -0 -f tape@dumphost:/dev/null /boot

DUMP: Connection to dumphost established.
  DUMP: cannot exec /bin/ssh -o BatchMode=yes -q -x -a -T -e none -i /etc/opt/backup/keys/backup_example: No such file or directory
```

The source confirms this:

```` c
dump-0.4b47/common/dumprmt.c

157    rmtgetconn(void)
....
164            const char *rsh;
....
170            rsh = getenv("RSH");
....
211            if (rsh) {
212                    const char *rshcmd[6];
213                    rshcmd[0] = rsh;
....
228                    if ((rshpid = piped_child(rshcmd)) < 0) {


461    int piped_child(const char **command) {
....
497                    execvp (command[0], (char *const *) command);
498                    msg("cannot exec %s: %s\n", command[0], strerror(errno));

````

## Also see
The source of the */etc/rmt*  used in CentOS 7 is from Schily Tools <http://schilytools.sourceforge.net> in particular *star* <http://cdrtools.sourceforge.net/private/star.html>

## Bugs

Probably many but something that might surprise:

the client library used *xfsdump*  (librmt) cannot handle long paths such as 

````bash
tape@dumphost:/dumps/servers/centos7/tintagieu/tintagieu-root-dump-2019-07-01_23:07:34.dump
````

whereas *dump* and *gnu tar* appear to have no problem.

The most obvious problem is the *sprintf(3)* in **_rmt_open()** which will overflow *buffer[]* with *device* without warning.

````C
# xfsdump-3.0.4/librmt/rmtlib.h
49    #define BUFMAGIC	64

# xfsdump-3.0.4/librmt/rmtopen.c
85    static int _rmt_open (char *path, int oflag, int mode)
....
88            char buffer[BUFMAGIC];
...
90            char device[BUFMAGIC];
....
268            sprintf(buffer, "O%s\n%d\n", device, oflag);
````

but this will overflow *device[]* first unless **BUFMAGIC** &gt; strlen(device)* &gt; **BUFMAGIC - 4**

````C
# xfsdump-3.0.4/librmt/rmtopen.c
 150            while (*path) {
 151                    *dev++ = *path++;
                }
 153            *dev = '\0';
````

