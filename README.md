xmemUSBFS
=========

FAT 16/32 module for xmemUSB

PLEASE NOTE:
f-variants, e.g. fopen(), can't be implemented due to the limitations of the
Arduino libc.
The C API is not completed yet, thus functionality at this point is minimal.

Currently available standard libc calls:
open()
creat()
read()
write()
close()

More coming soon, I just need to finish writing the wrappers.

