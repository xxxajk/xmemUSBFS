/*
 * File:   capi.h
 * Author: root
 *
 * Created on March 4, 2014, 11:07 PM
 */

#ifndef CAPI_H
#define	CAPI_H

#ifdef	__cplusplus
extern "C" {
#endif

 /* This is the offset value for a file system file descriptor  */
#define FD_VALUE_OFFSET 128

#include <stddef.h>
#include <ctype.h>
#include <time.h>
#include <xmemUSBFSdefs.h>
#include "fcntl.h"
#include <stdlib.h>

#if defined(AVR)
#define ARE_THERE_STILL_ENVS_WITHOUT_SYS_TYPES
#include <inttypes.h>
#include <stddef.h>
        typedef unsigned long off_t;
        typedef long ssize_t;
        typedef uint32_t mode_t;
#endif
extern int USBFAT_FSopen(const char *pathname, int flags);
extern int USBFAT_FScreat(const char *pathname, mode_t mode);
extern int USBFAT_FSclose(int fd);
extern int USBFAT_FSread(int fd, void *buf, size_t count);
extern int USBFAT_FSwrite(int fd, const void *buf, size_t count);
extern int USBFAT_FSfsync(int fd);
extern int USBFAT_FSftruncate(int fd, off_t length);
extern off_t USBFAT_FSlseek(int fd, off_t offset, int whence);
extern int USBFAT_FSutime(const char *filename, time_t *time);
extern int USBFAT_FSrename(const char *oldpath, const char *newpath);
extern int USBFAT_FSunlink(const char *pathname);
extern int USBFAT_FSmkdir(const char *pathname, mode_t mode);
extern void USBFAT_FSsync(void);
extern off_t USBFAT_FStell(int fd);
extern uint8_t USBFAT_FSeof(int fd);
extern uint8_t USBFAT_FSstat(const char *path, FILINFO *buf);
extern uint8_t USBFAT_FSchmod(const char *path, uint8_t mode);
extern int USBFAT_FSopendir(const char *path);
extern int USBFAT_FSclosedir(uint8_t dh);
extern int USBFAT_FSreaddir(uint8_t fd, DIRINFO *data);

extern int open(const char *pathname, int flags);
extern int creat(const char *pathname, mode_t mode);
extern int close(int fd);
extern int read(int fd, void *buf, size_t count);
extern int write(int fd, const void *buf, size_t count);
extern int fsync(int fd);
extern int ftruncate(int fd, off_t length);
extern off_t lseek(int fd, off_t offset, int whence);
extern int utime(const char *filename, time_t *time);
extern int rename(const char *oldpath, const char *newpath);
extern int unlink(const char *pathname);
extern int mkdir(const char *pathname, mode_t mode);
extern void sync(void);
extern off_t tell(int fd);
extern uint8_t eof(int fd);
extern uint8_t stat(const char *path, FILINFO *buf);
extern uint8_t chmod(const char *path, uint8_t mode);
extern int opendir(const char *path);
extern int closedir(uint8_t dh);
extern int readdir(uint8_t fd, DIRINFO *data);

#ifdef	__cplusplus
}
#endif

#endif	/* CAPI_H */

