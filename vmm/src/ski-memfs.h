/*
 * SKI - Systematic Kernel Interleaving explorer (http://ski.mpi-sws.org)
 *
 * Copyright (c) 2013-2015 Pedro Fonseca
 *
 *
 * This work is licensed under the terms of the GNU GPL, version 3.  See
 * the GPL3 file in SKI's top-level directory.
 *
 */


#ifndef MEMFS_H
#define MEMFS_H

//#define MEMFS_DISABLED 1

#ifndef MEMFS_DISABLED

#include <stdio.h>
#include <sys/select.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// =======================================================================================================================
// =======================================================================================================================
// XXX: IF THIS FILE IS MODIFIED ==> NEED TO DO CLEAN RECOMPILE TO SKI (make clean) BECAUSE THE DEPENDENCIES ARE NOT DETECTED BY QEMU's BUILD SYSTEM
// =======================================================================================================================
// =======================================================================================================================

#define MEMFS_HOOK(function)\
    memfs_hook_##function


#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>


FILE *MEMFS_HOOK(fopen)(const char *path, const char *mode);
int MEMFS_HOOK(fclose)(FILE *fp);
size_t MEMFS_HOOK(fread)(void *ptr, size_t size, size_t nmemb, FILE *fp);
size_t MEMFS_HOOK(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *fp);
void MEMFS_HOOK(clearerr)(FILE *stream);
int MEMFS_HOOK(feof)(FILE *stream);
int MEMFS_HOOK(ferror)(FILE *stream);
int MEMFS_HOOK(fileno)(FILE *stream);
int MEMFS_HOOK(fseek)(FILE *stream, long offset, int whence);
long MEMFS_HOOK(ftell)(FILE *stream);
void MEMFS_HOOK(rewind)(FILE *stream);
int MEMFS_HOOK(fgetpos)(FILE *stream, fpos_t *pos);
int MEMFS_HOOK(fsetpos)(FILE *stream, __const fpos_t *pos);
int MEMFS_HOOK(open) (__const char *pathname, int flags, ...);
int MEMFS_HOOK(creat)(const char *pathname, mode_t mode);
int MEMFS_HOOK(close)(int fd);
ssize_t MEMFS_HOOK(read)(int fd, void *buf, size_t count);
ssize_t MEMFS_HOOK(pread)(int fd, void *buf, size_t count, off_t offset);
ssize_t MEMFS_HOOK(write)(int fd, const void *buf, size_t count);
ssize_t MEMFS_HOOK(pwrite)(int fd, const void *buf, size_t count, off_t offset);
int MEMFS_HOOK(truncate)(const char *path, off_t length);
int MEMFS_HOOK(ftruncate)(int fd, off_t length);
off_t MEMFS_HOOK(lseek)(int fd, off_t offset, int whence);
loff_t MEMFS_HOOK(llseek)(int fd, loff_t offset, int whence);
int MEMFS_HOOK(select)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int MEMFS_HOOK(pselect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask);
//int MEMFS_HOOK(stat)(const char *path, struct stat *buf);
int MEMFS_HOOK(stat)(const char *path, struct stat *buf);
int MEMFS_HOOK(fstat)(int fd, struct stat *buf);
int MEMFS_HOOK(lstat)(const char *path, struct stat *buf);
int MEMFS_HOOK(dup)(int oldfd);
int MEMFS_HOOK(dup2)(int oldfd, int newfd);
int MEMFS_HOOK(dup3)(int oldfd, int newfd, int flags);
int MEMFS_HOOK(openat) (int __fd, __const char *__file, int __oflag, ...);
ssize_t MEMFS_HOOK(readv)(int fd, const struct iovec *iov, int iovcnt);
ssize_t MEMFS_HOOK(writev)(int fd, const struct iovec *iov, int iovcnt);

int MEMFS_HOOK(fcntl)(int fd, int cmd, ...  );
char *MEMFS_HOOK(fgets)(char *s, int size, FILE *stream);
int MEMFS_HOOK(fprintf)(FILE *stream, const char *format, ...);



/*
FILE *MEMFS_HOOK(fopen)(const char *path, const char *mode);
size_t MEMFS_HOOK(fread)(void *ptr, size_t size, size_t nmemb, FILE *fp);
size_t MEMFS_HOOK(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *fp);
long MEMFS_HOOK(ftell)(FILE *stream);
ssize_t MEMFS_HOOK(read)(int fd, void *buf, size_t count);
ssize_t MEMFS_HOOK(pread)(int fd, void *buf, size_t count, off_t offset);
ssize_t MEMFS_HOOK(write)(int fd, const void *buf, size_t count);
ssize_t MEMFS_HOOK(pwrite)(int fd, const void *buf, size_t count, off_t offset);
off_t MEMFS_HOOK(lseek)(int fd, off_t offset, int whence);
loff_t MEMFS_HOOK(llseek)(int fd, loff_t offset, int whence);
ssize_t MEMFS_HOOK(readv)(int fd, const struct iovec *iov, int iovcnt);
ssize_t MEMFS_HOOK(writev)(int fd, const struct iovec *iov, int iovcnt);

char *MEMFS_HOOK(fgets)(char *s, int size, FILE *stream);
*/


/*
FILE *MEMFS_HOOK(fopen)(const char *path, const char *mode);
int MEMFS_HOOK(fclose)(FILE *fp);
size_t MEMFS_HOOK(fread)(void *ptr, size_t size, size_t nmemb, FILE *fp);
size_t MEMFS_HOOK(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *fp);
void MEMFS_HOOK(clearerr)(FILE *stream);
int MEMFS_HOOK(feof)(FILE *stream);
int MEMFS_HOOK(ferror)(FILE *stream);
int MEMFS_HOOK(fileno)(FILE *stream);
int MEMFS_HOOK(fseek)(FILE *stream, long offset, int whence);
long MEMFS_HOOK(ftell)(FILE *stream);
void MEMFS_HOOK(rewind)(FILE *stream);
int MEMFS_HOOK(fgetpos)(FILE *stream, fpos_t *pos);
int MEMFS_HOOK(fsetpos)(FILE *stream, __const fpos_t *pos);
int MEMFS_HOOK(open) (__const char *pathname, int flags, ...);
int MEMFS_HOOK(creat)(const char *pathname, mode_t mode);
int MEMFS_HOOK(close)(int fd);
ssize_t MEMFS_HOOK(read)(int fd, void *buf, size_t count);
ssize_t MEMFS_HOOK(pread)(int fd, void *buf, size_t count, off_t offset);
ssize_t MEMFS_HOOK(write)(int fd, const void *buf, size_t count);
ssize_t MEMFS_HOOK(pwrite)(int fd, const void *buf, size_t count, off_t offset);
int MEMFS_HOOK(truncate)(const char *path, off_t length);
int MEMFS_HOOK(ftruncate)(int fd, off_t length);
off_t MEMFS_HOOK(lseek)(int fd, off_t offset, int whence);
loff_t MEMFS_HOOK(llseek)(int fd, loff_t offset, int whence);
int MEMFS_HOOK(select)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int MEMFS_HOOK(pselect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask);
int stat(const char *path, struct stat *buf);
int MEMFS_HOOK(stat)(const char *path, struct stat *buf);
int MEMFS_HOOK(fstat)(int fd, struct stat *buf);
int MEMFS_HOOK(lstat)(const char *path, struct stat *buf);
int MEMFS_HOOK(dup)(int oldfd);
int MEMFS_HOOK(dup2)(int oldfd, int newfd);
int MEMFS_HOOK(dup3)(int oldfd, int newfd, int flags);
int MEMFS_HOOK(openat) (int __fd, __const char *__file, int __oflag, ...);
ssize_t MEMFS_HOOK(readv)(int fd, const struct iovec *iov, int iovcnt);
ssize_t MEMFS_HOOK(writev)(int fd, const struct iovec *iov, int iovcnt);

*/


// Rely on the interception of the library functions
//#define read(...) memfs_hook_read(__VA_ARGS__)
//#define write(...) memfs_hook_write(__VA_ARGS__)
//#define lstat(...) memfs_hook_lstat(__VA_ARGS__)

#define fopen(...) memfs_hook_fopen(__VA_ARGS__)
#define fclose(...) memfs_hook_fclose(__VA_ARGS__)
#define fread(...) memfs_hook_fread(__VA_ARGS__)
#define fwrite(...) memfs_hook_fwrite(__VA_ARGS__)
#define clearerr(...) memfs_hook_clearerr(__VA_ARGS__)
#define feof(...) memfs_hook_feof(__VA_ARGS__)
#define ferror(...) memfs_hook_ferror(__VA_ARGS__)
#define fileno(...) memfs_hook_fileno(__VA_ARGS__)
#define fseek(...) memfs_hook_fseek(__VA_ARGS__)
#define ftell(...) memfs_hook_ftell(__VA_ARGS__)
#define rewind(...) memfs_hook_rewind(__VA_ARGS__)
#define fgetpos(...) memfs_hook_fgetpos(__VA_ARGS__)
#define fsetpos(...) memfs_hook_fsetpos(__VA_ARGS__)
#define open(...) memfs_hook_open(__VA_ARGS__)
#define creat(...) memfs_hook_creat(__VA_ARGS__)
#define close(...) memfs_hook_close(__VA_ARGS__)
#define pread(...) memfs_hook_pread(__VA_ARGS__)
#define pwrite(...) memfs_hook_pwrite(__VA_ARGS__)
#define truncate(...) memfs_hook_truncate(__VA_ARGS__)
#define ftruncate(...) memfs_hook_ftruncate(__VA_ARGS__)
#define lseek(...) memfs_hook_lseek(__VA_ARGS__)
#define llseek(...) memfs_hook_llseek(__VA_ARGS__)
#define select(...) memfs_hook_select(__VA_ARGS__)
#define pselect(...) memfs_hook_pselect(__VA_ARGS__)
#define stat(...) memfs_hook_stat(__VA_ARGS__)
#define fstat(...) memfs_hook_fstat(__VA_ARGS__)
#define dup(...) memfs_hook_dup(__VA_ARGS__)
#define dup2(...) memfs_hook_dup2(__VA_ARGS__)
#define dup3(...) memfs_hook_dup3(__VA_ARGS__)
#define openat(...) memfs_hook_openat(__VA_ARGS__)
#define readv(...) memfs_hook_readv(__VA_ARGS__)
#define writev(...) memfs_hook_writev(__VA_ARGS__)

#define fcntl(...) memfs_hook_fcntl(__VA_ARGS__)
#define fgets(...) memfs_hook_fgets(__VA_ARGS__)
#define fprintf(...) memfs_hook_fprintf(__VA_ARGS__)

#endif // MEMFS_DISABLED

#endif // MEMFS_H
