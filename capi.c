/*
 * File:   capi.c
 * Author: root
 *
 * Created on March 4, 2014, 11:07 PM
 */

#include "FScapi.h"


/**
 *
 * @param pathname path to file
 * @param flags Zero is treated as O_RDONLY, one of [0 | O_RDONLY | O_WRONLY | O_RDWR] is mandatory.
 *              Additional flags are  O_CREAT, O_APPEND and O_TRUNC.
 *              If you use O_RDONLY and O_CREAT together, it will be opened as O_RDWR.
 * @return file file system descriptor
 */
int _FSopen(const char *pathname, int flags) {
        int rv;
        char mode;
        char mode2;
        uint8_t s;
        // mode r
        // mode w/W
        // mode x/X (r/w)
        // W and X always create the file.
        if(!flags) flags = O_RDONLY;
        if((flags & (O_RDWR|O_CREAT)) == (O_RDONLY|O_CREAT)) flags |= O_WRONLY;
        switch(flags & (O_RDWR|O_CREAT)) {
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
                rv+=FD_VALUE_OFFSET;
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
int _FScreat(const char *pathname, mode_t mode) {
        return open(pathname, O_CREAT | O_WRONLY | O_TRUNC);
}

/**
 * Close file
 * @param fd file system file descriptor
 * @return 0 on success, -1 on fail
 */
int _FSclose(int fd) {
        uint8_t rv;
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        rv = fs_close(fd-FD_VALUE_OFFSET);
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
int _FSread(int fd, void *buf, size_t count) {
        int rv;
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        rv = fs_read(fd-FD_VALUE_OFFSET, buf, count);
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
int _FSwrite(int fd, const void *buf, size_t count) {
        int rv;
        if(fd < FD_VALUE_OFFSET) {
                return -1;
        }
        rv = fs_write(fd-FD_VALUE_OFFSET, buf, count);
        return rv;
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

int __attribute__((weak)) open(const char *pathname, int flags) {
        return _FSopen(pathname, flags);
}

/**
 * Create a file, truncate if it exists.
 *
 * @param pathname path to file
 * @param mode unused, kept for compatability
 * @return file descriptor
 */
int __attribute__((weak)) creat(const char *pathname, mode_t mode) {
        return _FScreat(pathname, mode);
}
/**
 * Close file
 * @param fd file descriptor
 * @return 0 on success, -1 on fail
 */
int __attribute__((weak)) close(int fd) {
        return _FSclose(fd);
}
/**
 * Read data
 *
 * @param fd file descriptor to read from
 * @param buf pre-allocated buffer to place data in to
 * @param count maximum amount of data to read
 * @return amount of data read, less than 0 indicates error
 */
int __attribute__((weak)) read(int fd, void *buf, size_t count) {
        return _FSread(fd, buf, count);
}
/**
 * Write data
 *
 * @param fd file descriptor to write to
 * @param buf buffer containing data to write
 * @param count how many bytes to write
 * @return count of bytes written, less than 0 indicates error
 */
int __attribute__((weak)) write(int fd, const void *buf, size_t count) {
        return _FSwrite(fd, buf, count);
}
