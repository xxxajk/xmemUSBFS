/*
 * File:   capi.c
 * Author: root
 *
 * Created on March 4, 2014, 11:07 PM
 */

#include "FScapi.h"
#define ATR __attribute__((weak))
/**
 *
 * @param pathname path to file
 * @param flags Zero is treated as O_RDONLY, one of [0 | O_RDONLY | O_WRONLY | O_RDWR] is mandatory.
 *              Additional flags are  O_CREAT, O_APPEND and O_TRUNC.
 *              If you use O_RDONLY and O_CREAT together, it will be opened as O_RDWR.
 * @return file file system descriptor
 */
int USBFAT_FSopen(const char *pathname, int flags) {
        int rv;
        char mode;
        char mode2;
        uint8_t s;
        // mode r
        // mode w/W
        // mode x/X (r/w)
        // W and X always create the file.
        if(!flags) flags = O_RDONLY;
        if((flags & (O_RDWR | O_CREAT)) == (O_RDONLY | O_CREAT)) flags |= O_WRONLY;
        switch(flags & (O_RDWR)) {
                case O_RDONLY:
                        mode = 'r';
                        break;

                case O_WRONLY:
                        mode = 'w';
                        mode2 = 'W';
                        break;

                case O_RDWR:
                        mode = 'x';
                        mode2 = 'X';
                        break;

                default:
                        return -1;
        }
        rv = fs_open(pathname, &mode);
        if((rv == 0) && (flags & O_CREAT)) {
                // try to create
                rv = fs_open(pathname, &mode2);
        }
        if(rv != 0) {
                if(flags & O_WRONLY) {
                        switch(flags & (O_APPEND | O_TRUNC)) {
                                case O_APPEND:
                                        // seek to eof
                                        s = fs_lseek(rv, 0, SEEK_END);
                                        if(s) {
                                                fs_close(rv);
                                                rv = -1;
                                        }
                                        break;
                                case O_TRUNC:
                                        // truncate file
                                        s = fs_truncate(rv);
                                        if(s) {
                                                fs_close(rv);
                                                rv = -1;
                                        }
                                        break;
                        }
                }
        } else {
                rv = -1;
        }
        if(rv != -1) {
                rv += FD_VALUE_OFFSET;
        }
        return rv;
}

/**
 * Create a file, truncate if it exists.
 *
 * @param pathname path to file
 * @param mode unused, kept for compatability
 * @return file file system descriptor
 */
int USBFAT_FScreat(const char *pathname, mode_t mode) {
        return open(pathname, O_CREAT | O_WRONLY | O_TRUNC);
}

/**
 * Close file
 * @param fd file system file descriptor
 * @return 0 on success, -1 on fail
 */
int USBFAT_FSclose(int fd) {
        int rv;
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        rv = fs_close(fd - FD_VALUE_OFFSET);
        if(rv) return -1;
        return 0;
}

/**
 * Read data
 *
 * @param fd file system file descriptor to read from
 * @param buf pre-allocated buffer to place data in to
 * @param count maximum amount of data to read
 * @return amount of data read, less than 0 indicates error
 */
int USBFAT_FSread(int fd, void *buf, size_t count) {
        int rv;
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        rv = fs_read(fd - FD_VALUE_OFFSET, buf, count);
        return rv;
}

/**
 * Write data
 *
 * @param fd file system file descriptor to write to
 * @param buf buffer containing data to write
 * @param count how many bytes to write
 * @return count of bytes written, less than 0 indicates error
 */
int USBFAT_FSwrite(int fd, const void *buf, size_t count) {
        int rv;
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        rv = fs_write(fd - FD_VALUE_OFFSET, buf, count);
        return rv;
}

int USBFAT_FSfsync(int fd) {
        int rv;
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        rv = fs_flush(fd - FD_VALUE_OFFSET);

}

off_t USBFAT_FSlseek(int fd, off_t offset, int whence) {
        if(fd < FD_VALUE_OFFSET) {
                return ((off_t)-1);
        }
        if(!fs_lseek(fd - FD_VALUE_OFFSET, offset, whence)) {
                return fs_tell(fd - FD_VALUE_OFFSET);
        }
        return ((off_t)-1);
}

int USBFAT_FSftruncate(int fd, off_t length) {
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        if(USBFAT_FSlseek(fd, length, SEEK_SET) != ((off_t) - 1)) {
                if(!fs_truncate(fd - FD_VALUE_OFFSET)) return 0;
        }
        return -1;
}

int USBFAT_FSutime(const char *filename, time_t *time) {
        if(!fs_utime(filename, time)) return 0;
        return -1;
}

int USBFAT_FSrename(const char *oldpath, const char *newpath) {
        if(!fs_rename(oldpath, newpath)) return 0;
        return -1;
}

int USBFAT_FSunlink(const char *pathname) {
        if(!fs_unlink(pathname)) return 0;
        return -1;
}

int USBFAT_FSmkdir(const char *pathname, mode_t mode) {
        if(!fs_mkdir(pathname, mode)) return 0;
        return -1;
}

void USBFAT_FSsync(void) {
        fs_sync();
}

off_t USBFAT_FStell(int fd) {
        if(fd < FD_VALUE_OFFSET) {
                return ((off_t)-1);
        }
        return fs_tell(fd - FD_VALUE_OFFSET);
}

uint8_t USBFAT_FSeof(int fd) {
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        return fs_eof(fd - FD_VALUE_OFFSET);
}

uint8_t USBFAT_FSstat(const char *path, FILINFO *buf) {
        return fs_stat(path, buf);
}

uint8_t USBFAT_FSchmod(const char *path, uint8_t mode) {
        return fs_chmod(path, mode);
}

int USBFAT_FSopendir(const char *path) {
        int rv = fs_opendir(path);
        if(rv) return rv + FD_VALUE_OFFSET;
        return -1;
}

int USBFAT_FSclosedir(uint8_t dh) {
        if(dh < FD_VALUE_OFFSET) {
                return -1;
        }
        return fs_closedir(dh - FD_VALUE_OFFSET);
}

int USBFAT_FSreaddir(uint8_t fd, DIRINFO *data) {
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        return fs_readdir(fd- FD_VALUE_OFFSET, data);
}

/* WRAPPERS to override */

/**
 *
 * @param pathname path to file
 * @param flags Zero is treated as O_RDONLY, one of [0 | O_RDONLY | O_WRONLY | O_RDWR] is mandatory.
 *              Additional flags are  O_CREAT, O_APPEND and O_TRUNC.
 *              If you use O_RDONLY and O_CREAT together, it will be opened as O_RDWR.
 * @return file descriptor
 */

ATR int open(const char *pathname, int flags) {
        return USBFAT_FSopen(pathname, flags);
}

/**
 * Create a file, truncate if it exists.
 *
 * @param pathname path to file
 * @param mode unused, kept for compatability
 * @return file descriptor
 */
ATR int creat(const char *pathname, mode_t mode) {
        return USBFAT_FScreat(pathname, mode);
}

/**
 * Close file
 * @param fd file descriptor
 * @return 0 on success, -1 on fail
 */
ATR int close(int fd) {
        return USBFAT_FSclose(fd);
}

/**
 * Read data
 *
 * @param fd file descriptor to read from
 * @param buf pre-allocated buffer to place data in to
 * @param count maximum amount of data to read
 * @return amount of data read, less than 0 indicates error
 */
ATR int read(int fd, void *buf, size_t count) {
        if(fd == 0) {
                int i=0;
                unsigned char *b=(char *)buf;
                while(i<count) {
                        *b=fgetc(stdin)&0xff;
                        b++;
                        i++;
                }
                return count;
        }
        return USBFAT_FSread(fd, buf, count);
}

/**
 * Write data
 *
 * @param fd file descriptor to write to
 * @param buf buffer containing data to write
 * @param count how many bytes to write
 * @return count of bytes written, less than 0 indicates error
 */
ATR int write(int fd, const void *buf, size_t count) {
        if(fd < 3) {
                int i=0;
                unsigned char *b=(char *)buf;
                while(i < count) {
                        if(fd == 1) fputc(*b, stdout);
                        if(fd == 2) fputc(*b, stderr);
                        b++;
                        i++;
                }
                return count;
        }
        return USBFAT_FSwrite(fd, buf, count);
}

ATR int fsync(int fd) {
        return USBFAT_FSfsync(fd);
}

ATR int ftruncate(int fd, off_t length) {
        return USBFAT_FSftruncate(fd, length);
}

ATR off_t lseek(int fd, off_t offset, int whence) {
        return USBFAT_FSlseek(fd, offset, whence);
}

ATR int utime(const char *filename, time_t *time) {
        return USBFAT_FSutime(filename, time);
}

ATR int rename(const char *oldpath, const char *newpath) {
        return USBFAT_FSrename(oldpath, newpath);
}

ATR int unlink(const char *pathname) {
        return USBFAT_FSunlink(pathname);
}

ATR int mkdir(const char *pathname, mode_t mode) {
        return USBFAT_FSmkdir(pathname, mode);
}

ATR void sync(void) {
        return USBFAT_FSsync();
}

ATR off_t tell(int fd) {
        return USBFAT_FStell(fd);
}

ATR uint8_t eof(int fd) {
        return USBFAT_FSeof(fd);
}

ATR uint8_t stat(const char *path, FILINFO *buf) {
        return USBFAT_FSstat(path, buf);
}

ATR uint8_t chmod(const char *path, uint8_t mode) {
        return USBFAT_FSchmod(path, mode);
}

ATR int opendir(const char *path) {
        return USBFAT_FSopendir(path);
}

ATR int closedir(uint8_t dh) {
        return USBFAT_FSclosedir(dh);
}

ATR int readdir(uint8_t fd, DIRINFO *data) {
        return USBFAT_FSreaddir(fd, data);
}

