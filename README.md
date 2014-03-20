xmemUSBFS
=========

FAT 16/32 module for <A HREF='https://github.com/xxxajk/xmemUSB'>xmemUSB</A>

LIMITATIONS:<BR>
Some f-variants, e.g. fopen(), can't be implemented due to the limitations of the
Arduino libc FILE struct.<BR>
All paths must be absolute and canonical.<BR>

Currently available standard libc calls:
open
creat
read
write
close
fsync
ftruncate
lseek
utime
rename
unlink
mkdir
sync
<BR>
FAT FS specific (Adjustments for porting code needed):<BR>
opendir
closedir
readdir
stat
chmod
<BR>
Non-standard:<BR>
tell, there is no standard C 'tell' for a plain fd<BR>
eof, there is no standard C 'eof' for a plain fd<BR>
fs_ready / fs_getfree, could make into int statvfs(const char *path, struct statvfs *buf);<BR>
fs_mount_lbl<BR>
fs_mountcount<BR>

TO-DO: fstat, and possibly others as-needed/requested
