/*
 * File:   xmemUSBFSdefs.h
 * Author: root
 *
 * Created on July 8, 2013, 7:51 AM
 */

#ifndef XMEMUSBFS_H
#define	XMEMUSBFS_H

#include <xmemUSBFSdefs.h>

/*
 *
 * These are not needed, we use absolute paths, and can track current path per task and track our own errors.
 *    f_chdrive - Change current drive
 *    f_chdir - Change current directory
 *    f_getcwd - Retrieve the current directory
 *    f_error - Test for an error on a file
 *
 * Nice idea if you are just using C, but we aren't, and I'm not about to write a callback wrapper.
 *   f_forward - Forward file data to the stream directly
 *
 * Abstacted
 *    f_getlabel - Get volume label
 *
 * Use libc if you need these, and connect them to read and write via libc iobyte.
 * If you absolutely require the data mangles that happen in these functions, write your own wrapper.
 *    f_printf - Write a formatted string
 *    f_gets - Read a string
 *    f_putc - Write a character
 *    f_puts - Write a string
 *
 * Not sure if I want to do these, you would have to know the logical drive info
 * ahead of time. Mounting is automatically handled too.
 *
 *    f_fdisk - Divide a physical drive -- Not available
 *    f_mkfs - Create a file system on the drive
 *    f_setlabel - Set volume label
 *    f_mount - Register/Unregister a work area
 *
 *
 */

namespace GenericFileSystem {
        int Setup(void);
        int Init(void);
        int Poll(uint8_t current_state, uint8_t last_state);
        int USB_Module_Call(uint8_t oper, uint8_t pargA, uint8_t pargB);
}



#endif	/* XMEMUSBFSDEFS_H */

