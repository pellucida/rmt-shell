Reimplementation of /etc/rmt


Added controlled access to file system
<br/>
eg restrict to /dev/nrst0


Added captive shell function so captive account
<br/>
eg tape can have rmt as a shell.


So remote clients eg dump, gtar will invoke /sbin/rmt -c /etc/rmt


Some remote client also do $SHELL -c uname  so added as internal command
to the shell function of rmt.
<br/>
eg HPUX vxdump, Tru64 vdump and xfsdump
(We can lie ;)
