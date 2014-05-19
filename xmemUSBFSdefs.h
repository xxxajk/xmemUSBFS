/*
 * File:   xmemUSBFS.h
 * Author: root
 *
 * Created on July 8, 2013, 4:54 AM
 */

#ifndef XMEMUSBFSDEFS_H
#define	XMEMUSBFSDEFS_H

#include <FAT/FatFS/src/ff.h>
//#include <xmemUSB.h>
#include <xmem.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef XMEM_MULTIPLE_APP
// per-task fs error code
extern uint8_t fs_err[USE_MULTIPLE_APP_API];
#else
extern uint8_t fs_err;
#endif
typedef struct {
                DWORD fsize; /* File size */
                WORD fdate; /* Last modified date */
                WORD ftime; /* Last modified time */
                FBYTE fattrib; /* Attribute */
                TCHAR fname[13]; /* Short file name (8.3 format) */
                TCHAR lfname[_MAX_LFN + 1]; /* Start point of long file name. */
} __attribute__((packed)) DIRINFO;

uint8_t fs_ready(const char *path);
uint8_t fs_open(const char *path, const char *mode);
uint8_t fs_opendir(const char *path);
int fs_close(uint8_t fd);
int fs_closedir(uint8_t dh);
int fs_read(uint8_t fd, void *data, uint16_t amount);
int fs_readdir(uint8_t fd, DIRINFO *data);
int fs_write(uint8_t fd, const void *data, uint16_t amount);
uint8_t fs_unlink(const char *path);

uint8_t fs_sync(void);
uint8_t fs_flush(uint8_t fd);
uint8_t fs_eof(uint8_t fd);
uint8_t fs_truncate(uint8_t fd);

unsigned long fs_tell(uint8_t fd);
uint8_t fs_lseek(uint8_t fd, unsigned long offset, int whence);

uint8_t fs_rename(const char *oldpath, const char *newpath);
uint8_t fs_chmod(const char *path, uint8_t mode);
uint8_t fs_utime(const char *path, time_t timesec);
uint8_t fs_mkdir(const char *path, uint8_t mode);
uint64_t fs_getfree(const char *path);
uint8_t fs_stat(const char *path, FILINFO *buf);
char *fs_mount_lbl(uint8_t vol);
uint8_t fs_mountcount(void);
#ifdef __cplusplus
}
#endif
#endif	/* XMEMUSBFS_H */
