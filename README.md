### Reimplementation of */etc/rmt*


Added controlled access to file system locations
such as devices, files and directories
<br/>
eg restrict to /dev/nrst0 or /dev/nrmt* 


Added captive shell function in order a captive (tape)
account can use <i>rmt</i> as a login shell.
<br/>

Remote clients eg dump, gtar will invoke /sbin/rmt -c /etc/rmt

Some remote clients also do $RSH $SHELL -c uname 
so <i>uname</i> is an internal command rmt shell capability.
Programs like xfsdump use uname to determine the type of remote
system (Irix, Linux) which is then used to determine the mtio(4)
tape device driver op codes to perform rewinds etc.
<br/>
eg HPUX vxdump, Tru64 vdump and xfsdump
(We can lie ;)

Supports SSH forced commands by retrieving $SSH_ORIGINAL_COMMAND
from the environment.

