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

#if defined(AVR)
#define ARE_THERE_STILL_ENVS_WITHOUT_SYS_TYPES
#include <inttypes.h>
#include <stddef.h>
        typedef unsigned long off_t;
        typedef long ssize_t;
        typedef uint32_t mode_t;
#endif
int _FSopen(const char *pathname, int flags);
int _FScreat(const char *pathname, mode_t mode);
int _FSclose(int fd);
int _FSread(int fd, void *buf, size_t count);
int _FSwrite(int fd, const void *buf, size_t count);

#ifdef	__cplusplus
}
#endif

#endif	/* CAPI_H */

