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



#include "ski-memfs.h"


#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/syscall.h> 
#include <sys/stat.h>
#include <pthread.h>

#include <fcntl.h>
#include <stdarg.h>
#include <sys/vfs.h>

#include <sys/uio.h>
#include "ski-debug.h"

#ifndef MEMFS_DISABLED

#warning "Compling with memfs"


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

int MEMFS_HOOK(fcntl)(int fd, int cmd, ...);   //  arg *);
//char *MEMFS_HOOK(fgets)(char *s, int size, FILE *stream);
int MEMFS_HOOK(fprintf)(FILE *stream, const char *format, ...);
//int MEMFS_HOOK(fprintf)(FILE *stream, const char *format, ...);

*/

FILE* (*default_fopen) (const char *path, const char *mode);
int (*default_fclose) (FILE *fp);
size_t (*default_fread) (void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t (*default_fwrite) (const void *ptr, size_t size, size_t nmemb, FILE *stream);
void (*default_clearerr) (FILE *stream);
int (*default_feof) (FILE *stream);
int (*default_ferror)(FILE *stream);
int (*default_fileno) (FILE *stream);

int (*default_fseek) (FILE *stream, long offset, int whence);
long (*default_ftell) (FILE *stream);
void (*default_rewind) (FILE *stream);
int (*default_fgetpos) (FILE *stream, fpos_t *pos);
int (*default_fsetpos) (FILE *stream, fpos_t *pos);


//int (*default_open) (const char *pathname, int flags);
int (*default_open) (const char *pathname, int flags);
int (*default_open_mode) (const char *pathname, int flags, mode_t mode);
int (*default_creat) (const char *pathname, mode_t mode);
int (*default_close) (int fd);
ssize_t (*default_read) (int fd, void *buf, size_t count);
ssize_t (*default_pread) (int fd, void *buf, size_t count, off_t offset);
ssize_t (*default_write) (int fd, const void *buf, size_t count);
ssize_t (*default_pwrite) (int fd, const void *buf, size_t count, off_t offset);

int (*default_truncate) (const char *path, off_t length);
int (*default_ftruncate) (int fd, off_t length);
off_t (*default_lseek) (int fd, off_t offset, int whence);
loff_t (*default_llseek) (int fd, loff_t offset, int whence);
//int (*default__llseek) (unsigned int fd, unsigned long offset_high, unsigned long offset_low, loff_t *result, unsigned int whence);
off64_t (*default_lseek64) (int fd, off64_t offset, int whence);

int (*default_select) (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int (*default_pselect) (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask);

int (*default_stat) (const char *path, struct stat *buf);
//int (*default_stat64) (const char *path, struct stat *buf);
int (*default_fstat) (int fd, struct stat *buf);
int (*default_lstat) (const char *path, struct stat *buf);

int (*default_fcntl)(int fd, int cmd, ... /* arg */ );
char *(*default_fgets)(char *s, int size, FILE *stream);
int (*default_fprintf)(FILE *stream, const char *format, ...);

int (*default_dup)(int oldfd);
int (*default_dup2)(int oldfd, int newfd);
//int dup3(int oldfd, int newfd, int flags);

void (*default_statfs64)(const char *path, struct statfs64 *buf);
int (*default___lxstat64)(int __ver, __const char *__filename, struct stat64 *__stat_buf);



int (*default_timer_create)(clockid_t clockid, struct sigevent *evp, timer_t *timerid);
int (*default_timer_settime)(timer_t timerid, int flags, const struct itimerspec *new_value, struct itimerspec * old_value);
int (*default_timer_gettime)(timer_t timerid, struct itimerspec *curr_value);


#define MEMFS_LOG(level, format, args...)                                  \
{                                                                   \
	if(ski_memfs_log_level >= level){									\
		printf("[SKI] [MEMFS] ");											\
		printf(format, ##args);										\
	}																\
}



//	printf(#format, ## args);  

#define assert(x)	\
	SKI_ASSERT_MSG(x, SKI_EXIT_MEMFS, "MEMFS")

	
#define INITIALIZE_WRAPPER(x)				\
	default_##x = dlsym(RTLD_NEXT, #x);	\
	assert(default_##x);

#define DEBUG

#ifdef DEBUG
#define DEBUG_WRAPPER(x)\
	MEMFS_LOG(2, "Wrapper %s called\n", #x);
#else
#define DEBUG_WRAPPER(x)
#endif

#define MEMFS_MIN(x,y) (((x)<(y))?(x):(y))
#define MEMFS_MAX(x,y) (((x)>(y))?(x):(y))

#define MEMFS_MAX_FILENAME 256
#define MEMFS_MAX_FILES 50
#define MEMFS_MMAP_EXTRA_SPACE (512 * 1024 * 1024)
//#define MEMFS_MMAP_EXTRA_SPACE (64 * 1024 * 1024)

typedef struct struct_memfs_support{
	char filename[MEMFS_MAX_FILENAME];
	int inode;
	char *mmap_addr;
	size_t mmap_length;
	char *extra_addr;
	size_t extra_length;
	size_t length;
} memfs_support;

typedef struct struct_memfs_file
{
	int id;
	char filename[MEMFS_MAX_FILENAME];
	int fd;
	FILE *file;
	int closed;
	size_t off;
	memfs_support *sup;
} memfs_file;


memfs_support sups[MEMFS_MAX_FILES];
int sups_n = 0;

memfs_file files[MEMFS_MAX_FILES];
int files_n = 0;

#define MEMFS_FD_START 100



int ski_memfs_test_mode_enabled = 0;
int ski_memfs_enabled = 1;
int ski_memfs_log_level = 1;


void init_wrappers(void);

int str_ends_with(char* str, char *suffix);
int memfs_intercept_filename(char *pathname);
memfs_file* memfs_get_file(FILE* file);
memfs_file* memfs_get_fd(int fd);
memfs_file* memfs_get_filename(char *filename);
void memfs_dump_all(void);
int memfs_close(memfs_file *f);
memfs_support * memfs_support_open(char *path, int fd, FILE* file);
memfs_file *memfs_open(char *path, int fd, FILE*file);
size_t memfs_read(char *ptr, size_t size, size_t nmemb, size_t off, memfs_file* f, int update_off);
size_t memfs_write(char *ptr, size_t size, size_t nmemb, size_t off, memfs_file* f, int update_off);


/*==================================================================
                        MEMFS INITIALIZATION FUNCTION
==================================================================*/

void init_wrappers(void){
	static int wrappers_initialized = 0;

	if(wrappers_initialized){
		return;
	}

	//stat(const char *path, struct stat *buf)
	//fstat(int fd, struct stat *buf)
	//lstat(const char *path, struct stat *buf)
		
	
	// Stream ops
	INITIALIZE_WRAPPER(fopen);
	INITIALIZE_WRAPPER(fclose);
	INITIALIZE_WRAPPER(fread);
	INITIALIZE_WRAPPER(fwrite);
	INITIALIZE_WRAPPER(clearerr);
	INITIALIZE_WRAPPER(feof);
	INITIALIZE_WRAPPER(ferror);
	INITIALIZE_WRAPPER(fileno);

	INITIALIZE_WRAPPER(fseek);
	INITIALIZE_WRAPPER(ftell);
	INITIALIZE_WRAPPER(rewind);
	INITIALIZE_WRAPPER(fgetpos);
	INITIALIZE_WRAPPER(fsetpos);

	// Fd ops
	INITIALIZE_WRAPPER(open);
	default_open_mode = (int (*) (const char *pathname, int flags, mode_t mode)) default_open;
	INITIALIZE_WRAPPER(creat);
	INITIALIZE_WRAPPER(close);
	INITIALIZE_WRAPPER(read);
	INITIALIZE_WRAPPER(pread);
	INITIALIZE_WRAPPER(write);
	INITIALIZE_WRAPPER(pwrite);
	INITIALIZE_WRAPPER(truncate);
	INITIALIZE_WRAPPER(ftruncate);
	INITIALIZE_WRAPPER(lseek);
	//INITIALIZE_WRAPPER(_llseek);
	INITIALIZE_WRAPPER(lseek64);
	INITIALIZE_WRAPPER(select);
	INITIALIZE_WRAPPER(pselect);
	// Reeimplemented ops (because of the hacky way used by the glibc - static + dynamic linking)
	//INITIALIZE_WRAPPER(stat);
	//INITIALIZE_WRAPPER(fstat);
	//INITIALIZE_WRAPPER(lstat);

	INITIALIZE_WRAPPER(fcntl);
	INITIALIZE_WRAPPER(fgets);
	INITIALIZE_WRAPPER(fprintf);
	INITIALIZE_WRAPPER(dup);
	INITIALIZE_WRAPPER(dup2);

	INITIALIZE_WRAPPER(statfs64);
	INITIALIZE_WRAPPER(__lxstat64);

	INITIALIZE_WRAPPER(timer_create);
	INITIALIZE_WRAPPER(timer_settime);
	INITIALIZE_WRAPPER(timer_gettime);

	char *ski_memfs_test_mode_enabled_str = getenv("SKI_MEMFS_TEST_MODE_ENABLED");
	if(ski_memfs_test_mode_enabled_str){
		sscanf(ski_memfs_test_mode_enabled_str, "%d", &ski_memfs_test_mode_enabled);

	}

	char *ski_memfs_enabled_str = getenv("SKI_MEMFS_ENABLED");
	if(ski_memfs_enabled_str){
		sscanf(ski_memfs_enabled_str, "%d", &ski_memfs_enabled);

	}

	char *ski_memfs_log_level_str = getenv("SKI_MEMFS_LOG_LEVEL");
	if(ski_memfs_log_level_str){
		sscanf(ski_memfs_log_level_str, "%d", &ski_memfs_log_level);


	}


	MEMFS_LOG(0, "Initialized the wrappers\n");
	MEMFS_LOG(0, " Option ski_memfs_test_mode_enabled = %d\n", ski_memfs_test_mode_enabled);
	MEMFS_LOG(0, " Option ski_memfs_enabled = %d\n", ski_memfs_enabled);
	MEMFS_LOG(0, " Option ski_memfs_log_level = %d\n", ski_memfs_log_level);

	if(ski_memfs_test_mode_enabled){
		MEMFS_LOG(0, "WARNING: memfs is slower because test mode is enabled!!!!!\n");
	}

	if(ski_memfs_test_mode_enabled && !ski_memfs_enabled){
		//MEMFS_LOG(0, "ERROR: Test mode enabled but MEMFS disabled\n");
		SKI_ASSERT_MSG(0, SKI_EXIT_MEMFS, "MEMFS_TEST_MODE");
	}

	//stat(0, 0);
	//fstat(0, 0);
	//lstat(0, 0);

	// Innitializing structures
	memset(files, 0, MEMFS_MAX_FILES * sizeof(memfs_file));
	files_n = 0;

	wrappers_initialized = 1;
}

// FILE *fdopen(int fd, const char *mode);
// FILE *freopen(const char *path, const char *mode, FILE *stream);


//char *in_filenames[] = {"test.txt", "./file2.txt"};



/*	
static int is_file_inmem(char *path){
	int i;

	for(i=0; i<sizeof(in_filename_list)/sizeof(char*); i++){
		char* str = in_filename_list[i];
		if(strcmp(path,str) == 0){
			MEMFS_LOG(2, "Trying to open path %s which matches entry %d\n", path, i);
		}
	}
}

*/

/*==================================================================
					  MEMFS CORE FUNCTIONS
==================================================================*/




int str_ends_with(char* str, char *suffix){
	if(strlen(str)<strlen(suffix))
		return 0;
	if(strcmp(str + strlen(str) - strlen(suffix), suffix)==0)
		return 1;
	return 0;
}

int memfs_intercept_filename(char *pathname){
	char* suffix_1= (char*)".ipfilter";
	char* suffix_2= (char*)".txt"; // Trace file uses fprintf

	//char* suffix_3="common";

	if(str_ends_with(pathname, suffix_1)){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s'\n", pathname, suffix_1);
		return 0;
	}

	//if(str_ends_with(pathname, suffix_2) && strstr(pathname, "trace_")){
	if(strstr(pathname, "trace_")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'trace_'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "msg_")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'msg_'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "console_")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'console_'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "heuristics_")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'run_'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "run_")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'run_'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "forkall_")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'forkall'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "console")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'console'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "write")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'write'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "read")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'read'\n", pathname, suffix_2);
		return 0;
	}
	if(str_ends_with(pathname, suffix_2) && strstr(pathname, "exec")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it has suffix '%s' and contains 'exec'\n", pathname, suffix_2);
		return 0;
	}
	if(strstr(pathname, "/dev/null")){
		MEMFS_LOG(1, "WARNING: Ignoring filename '%s' because it contains '/dev/null'\n", pathname);
		return 0;
	}

	return 1;
}


char ignorelist[MEMFS_MAX_FILES][MEMFS_MAX_FILENAME];
int ignorelist_n = 0;

void memfs_ignorelist_add(char* filename){
	assert(ignorelist_n < MEMFS_MAX_FILES);
	assert(strlen(filename) < MEMFS_MAX_FILENAME);
	//printf("[***** Sishuai *****] adding %s into ignore list 0x%08x, whose length is %d\n", filename, &ignorelist, ignorelist_n);
	//if the file is read/write/exec, simply not adding it to the list
	if (strstr(filename, "exec") || strstr(filename, "write") || strstr(filename, "read")){
		return;
	}
	else{
		strcpy(ignorelist[ignorelist_n], filename);
		ignorelist_n++;
	}
}

void memfs_ignorelist_dump(){
	int i;

	for(i=0;i<ignorelist_n;i++){
		MEMFS_LOG(2, "Ignored filename: %s\n", ignorelist[i]);
	}
}

memfs_file* memfs_get_file(FILE* file){
	void* ptr = (void *)file;

	// Check that it's within range
	if((ptr >= (void*) &files[0]) && (ptr < (void*) &files[files_n])){
		// Assert that the pointer is aligned
		assert(((ptr - ((void *)files)) % sizeof(memfs_file)) == 0 );
		memfs_file* ret = (memfs_file*) ptr;
		assert(ret->closed==0);
		//MEMFS_LOG(2, "File found (%s)\n", ((memfs_file*)ptr)->filename);
		return ret; 
	}
	MEMFS_LOG(2, "File not found (FILE %p)\n", file);
	return 0;
}


memfs_file* memfs_get_fd(int fd){
	int f_index = fd - MEMFS_FD_START;

	// Check that it's within range
	if(f_index >= 0 && f_index < files_n){
		//MEMFS_LOG(2, "File found (%s)\n", ((memfs_file*)ptr)->filename);
		memfs_file* ret = (memfs_file*) &files[f_index];
		assert(ret->closed==0);
		return ret; 
	}
	if(fd != 6 && fd != 7 && fd != 8 && fd != 14 && fd != 15){
		MEMFS_LOG(2, "File not found suspect (fd %d)\n", fd);
	}
	MEMFS_LOG(2, "File not found (fd %d)\n", fd);
	return 0;
}

memfs_file* memfs_get_filename(char *filename){
	int f_index = 0;

	for(f_index = 0;f_index < files_n; f_index++){
		memfs_file* f = &files[f_index];
		if((f->closed == 0) && (strcmp(f->filename, filename)==0)){
			return f;
		}
	}
	MEMFS_LOG(2, "File not found (%s)\n", filename);
	//memfs_dump_all();
	return 0;
}

void memfs_dump_all(void){
	int f_index = 0;

	MEMFS_LOG(2, "memfs_dump_all (file_ns=%d):\n", files_n);

	for(f_index = 0;f_index < files_n; f_index++){
		memfs_file* f = &files[f_index];
		MEMFS_LOG(2, "Pointer=%p ID=%d filename=%s fd=%d FILE=%p length=%lld off=%lld mmap_length=%lld extra_length=%lld sup=%p closed=%d\n", 
			f, f->id, f->filename, f->fd, f->file, (long long int) f->sup->length, (long long int)f->off, (long long int)f->sup->mmap_length, (long long int)f->sup->extra_length, f->sup, f->closed);
	}
	
	memfs_ignorelist_dump();
}

int memfs_close(memfs_file *f){
	MEMFS_LOG(2, "memfs_close: closing the file (%s)\n", f->filename);
	// XXX: Do not close this because it could be (or end up becoming) shared
	//munmap(f->sup->mmap_addr, f->sup->mmap_length);
	//munmap(f->sup->extra_addr, f->sup->extra_length);

	// XXX: This leads to leaks but maybe good for debugging purposes
	if(f->file){
		//default_fclose(f->file);
	}else{
		//default_close(f->fd);
	}
	f->closed = 1;
	return 0;
}


memfs_support * memfs_support_open(char *path, int fd, FILE* file){
	MEMFS_LOG(2, "memfs_support_open: opened the file (%s)\n", path);

	if(fd == 0){
		assert(file);
		fd = fileno(file);
	}else{
		file = 0;
	}
	int fstat_res;
	struct stat stat_buf;
	//fstat_res = fstat(fd, &stat_buf);
	fstat_res = syscall(__NR_fstat, fd, &stat_buf);
	assert(!fstat_res);

	int inode = stat_buf.st_ino;
	int i;
	memfs_support *s = 0;
	for(i=0;i<sups_n;i++){
		s = &sups[i];
		if(s->inode == inode){
			// If we already have a suppport for this file, reuse it (it's shared between different memfs_file)
			return s;
		}
	}

	// Otherwise create a new support for this file
	assert(strlen((char *)path) < (MEMFS_MAX_FILENAME - 1));
	assert(sups_n < MEMFS_MAX_FILES);
	s = &sups[sups_n];

//	s->id = sups_n;
//	s->fd = fd;
//	s->file = file;
	strcpy(s->filename, (char*)path);
	s->inode = inode;


	size_t length =  stat_buf.st_size;
	MEMFS_LOG(2, "memfs_support_open: file has %lld bytes (sizeof(size_t) = %lu, sizeof(long long)=%lu)\n", (long long) length, sizeof(size_t), sizeof(long long));
	s->length = length;
	s->mmap_length = s->length;

	// XXX: For now allow allow accesses
	// XXX: Hope there are no races changin the file size?
	if(s->mmap_length>0){
		void* mmap_ret;
		mmap_ret = mmap(NULL, s->mmap_length, PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE, fd, 0);
		if(mmap_ret == MAP_FAILED){
			perror("mmap file error\n");
			assert(mmap_ret != MAP_FAILED);
		}
		s->mmap_addr = (char *)mmap_ret;
	}else{
		s->mmap_addr=0;
	}

	s->extra_length = MEMFS_MMAP_EXTRA_SPACE;
	s->extra_addr = (char *) mmap(NULL, s->extra_length, PROT_READ | PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if(s->extra_addr == MAP_FAILED){
		perror("mmap annon buffer error\n");
		assert(s->extra_addr != MAP_FAILED);
	}

//	s->off = 0;
//	s->closed = 0;
	sups_n++;

	MEMFS_LOG(2, "memfs_support_open: finished opening the support (%s)\n", path);
	
	memfs_dump_all();

	return s;
}

memfs_file *memfs_open(char *path, int fd, FILE*file){

	MEMFS_LOG(1, "memfs_open: opened the file (%s)\n", path);

	if(fd == 0){
		assert(file);
		fd = fileno(file);
	}else{
		file = 0;
	}

    assert(strlen((char *)path) < (MEMFS_MAX_FILENAME - 1));
	assert(sups_n < MEMFS_MAX_FILES);
	memfs_file *f = &files[files_n];
	f->id = files_n;
	f->fd = fd;
	f->file = file;
	strcpy(f->filename, (char*)path);
	f->off = 0;
	f->closed = 0;
	f->sup = memfs_support_open((char *)path, fd, 0);

	files_n++;

	//memfs_dump_all();

	return f;
}


size_t memfs_read(char *ptr, size_t size, size_t nmemb, size_t off, memfs_file* f, int update_off){
	size_t total_read_length = size * nmemb;
	MEMFS_LOG(2, "memfs_read ptr=%p size=%lld nmemb=%lld off=%lld f=%p f->length=%lld update_off=%d\n", ptr, (long long int)size, (long long int)nmemb, (long long int)off, f, (long long int)f->sup->length, update_off);

	// This fails if reading past the size of the file
	// assert(off <= f->sup->length);
	if(off> f->sup->length){
		MEMFS_LOG(2, "WARNING! memfs_read tried to read after the length of the file\n");
		return 0;
	}

	if(off == f->sup->length)
		return 0;

	if((off + total_read_length) > f->sup->length){
		// If reached the EOF, don't go beyond
		total_read_length = (f->sup->length - off) / size * size;
	}

	size_t mmap_read_length = 0;
	if(off < f->sup->mmap_length){
		// Have to read in the mmap block
		mmap_read_length = MEMFS_MIN(f->sup->mmap_length - off, total_read_length);
		memcpy(ptr, f->sup->mmap_addr + off, mmap_read_length);
	}

	size_t extra_read_length = 0;
	if(mmap_read_length != total_read_length){
		// Have to read in the extra block 
		extra_read_length = total_read_length - mmap_read_length;
		size_t extra_off = 0;
		if(mmap_read_length){
			// Start of at the begining of the extra block
			extra_off = 0;
		}else{
			// Start somewhere after the begining of the extra block
			assert(mmap_read_length == 0);
			extra_off = off - f->sup->mmap_length;
		}
		assert(extra_off + extra_read_length <= f->sup->extra_length);
		memcpy(ptr + mmap_read_length, f->sup->extra_addr + extra_off, extra_read_length);
	}
	
	if(update_off){
		f->off = off + total_read_length;
		//f->off += total_read_length;
	}

	return total_read_length;
}

// If ptr == NULL, zeroes new area instead of copying from the buffer pointed to by ptr
size_t memfs_write(char *ptr, size_t size, size_t nmemb, size_t off, memfs_file* f, int update_off){
	MEMFS_LOG(2, "memfs_write ptr=%p size=%lld nmemb=%lld off=%lld f=%p f->length=%lld update_off=%d\n", ptr, (long long int)size, (long long int)nmemb, (long long int)off, f, (long long int)f->sup->length, update_off);

	size_t total_write_length = size * nmemb;

	if(off > f->sup->length){
		// Extend with zeroes
		MEMFS_LOG(2, "memfs_write extension: off=%lld f->length=%lld\n", (long long int)off, (long long int)f->sup->length);
		size_t extension_bytes = off - f->sup->length;
		size_t bytes = memfs_write(0, extension_bytes, 1, f->sup->length, f, 0);
		MEMFS_LOG(2, "memfs_write extension: off=%lld f->length=%lld bytes=%lld\n", (long long int)off, (long long int)f->sup->length, (long long int)bytes);
		
		assert(off <= f->sup->length);
	}

	size_t mmap_write_length = 0;
	if(off < f->sup->mmap_length){
		// Have to write in the mmap block
		mmap_write_length = MEMFS_MIN(f->sup->mmap_length - off, total_write_length);
		if(ptr){
			memcpy(f->sup->mmap_addr + off, ptr, mmap_write_length);
		}else{
			memset(f->sup->mmap_addr + off, 0, mmap_write_length);
		}
	}

	if(mmap_write_length != total_write_length){
		// Have to write in the extra block 
		size_t extra_write_length = total_write_length - mmap_write_length;
		size_t extra_off = 0;
		if(mmap_write_length){
			// Start of at the begining of the extra block
			extra_off = 0;
		}else{
			// Start somewhere after the begining of the extra block
			assert(mmap_write_length == 0);
			extra_off = off - f->sup->mmap_length;
		}
		//assert(extra_off + extra_write_length <= f->sup->extra_length);
		SKI_ASSERT_MSG(extra_off + extra_write_length <= f->sup->extra_length, SKI_EXIT_MEMFS_LOWMEM, "MEMFS: Not enough space in the extra buff");

		if(ptr){
			memcpy(f->sup->extra_addr + extra_off, ptr + mmap_write_length, extra_write_length);
		}else{
			memset(f->sup->extra_addr + extra_off, 0, extra_write_length);
		}
	}

	f->sup->length = MEMFS_MAX(f->sup->length, off + total_write_length);
	
	if(update_off){
		f->off =  off + total_write_length;
		//f->off += total_write_length;
	}

	return total_write_length;
}

/*==================================================================
                        STREAM HOOK FUNCTIONS
==================================================================*/

FILE *MEMFS_HOOK(fopen)(const char *path, const char *mode){
	FILE* file;

	init_wrappers();
	
	DEBUG_WRAPPER(fopen)

	MEMFS_LOG(2, "fopen file %s\n", (char*)path);

	file = default_fopen(path, mode);

	if(!file){
		MEMFS_LOG(0, "WARNING: fopen file %s returned 0\n", (char*)path);
	}

	if(ski_memfs_enabled){
		if(!memfs_intercept_filename(path)){
			MEMFS_LOG(2, "fopen file %s (%p) -> ignoring\n", (char*)path, file);
			memfs_ignorelist_add(path);
			memfs_dump_all();
			return file;
		}

		if(file){
			assert(mode[0]!='a'); // NYI
			memfs_file *f = memfs_open((char *)path, 0, file);
			file = (FILE *) f;
		}else{
			MEMFS_LOG(2, "Warning: fopen failed to open the file (%s)\n", (char*)path);
			file = 0;
		}
		MEMFS_LOG(2, "fopen file %s (%p) -> intercepting\n", (char*)path, file);
	}

	memfs_dump_all();
	return file;
}


int MEMFS_HOOK(fclose)(FILE *fp){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(fclose)

	MEMFS_LOG(2, "fclose file (%p)\n", fp);

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(fp);
		if(f){
			ret = memfs_close(f);
			return ret;
		}
	}
	// XXX: Leak, we're leaving it open
	// Sishuai Now we close the file
	// what leak it might bring? maybe add a filter here, only fclose set file
	// only ignored file would reach here
	ret = default_fclose(fp);
	ret = 0;
	return ret;
}

#define MAX_TEST_BUFF (64*1024*1024)
char test_buff[MAX_TEST_BUFF];
pthread_mutex_t test_buff_lock = PTHREAD_MUTEX_INITIALIZER;

#define START_TEST(max)								\
	void* ptr_original = ptr;						\
													\
	if(ski_memfs_test_mode_enabled){				\
		pthread_mutex_lock(&test_buff_lock);		\
		ptr = test_buff;		        			\
		assert((max)<MAX_TEST_BUFF);					\
	}								

// XXX: we should actually only compare the size of what was actually read 
#define END_TEST(statment_original, res_expected, buf_length)   \
	if(ski_memfs_test_mode_enabled){							\
		int res_original = statment_original;					\
		assert(buf_length < MAX_TEST_BUFF);						\
		assert(res_original == res_expected);					\
		assert(0==memcmp(ptr, ptr_original, buf_length));		\
		pthread_mutex_unlock(&test_buff_lock);					\
	}



size_t MEMFS_HOOK(fread)(void *ptr, size_t size, size_t nmemb, FILE *fp){
	size_t ret;
	
	init_wrappers();
	DEBUG_WRAPPER(fread)

	if(ski_memfs_enabled){
		MEMFS_LOG(2, "fread: ptr: %p size: %lld nmemb: %lld fp: %p\n", ptr, (long long int)size, (long long int)nmemb, fp);
		memfs_file *f = memfs_get_file(fp);
		if(f){
			int update_off = 1;
	
			START_TEST(size * nmemb);
		
			size_t bytes = memfs_read((char*)ptr, size, nmemb, f->off, f, update_off);
			ret = bytes / size;

			END_TEST(default_fread(ptr_original, size, nmemb, f->file), ret, (ret>0)?ret*size:0);

			MEMFS_LOG(2, "  fread returned %lld\n", (long long int)ret);
			return ret;
		}
	}

	ret = default_fread(ptr, size, nmemb, fp);
	return ret;
}

size_t MEMFS_HOOK(fwrite)(const void *ptr, size_t size, size_t nmemb, FILE *fp){
	size_t ret;

	init_wrappers();
	DEBUG_WRAPPER(fwrite)
	if(ski_memfs_enabled){
		MEMFS_LOG(2, "fwrite: ptr: %p size: %lld nmemb: %lld fp: %p\n", ptr, (long long int)size,(long long int) nmemb, fp);

		memfs_file *f = memfs_get_file(fp);
		if(f){
			int update_off = 1;
		
			START_TEST(size * nmemb);

			size_t bytes = memfs_write((char*)ptr, size, nmemb, f->off, f, update_off);
			ret = bytes / size;

			END_TEST(default_fwrite(ptr_original, size, nmemb, f->file), ret, 0);

			MEMFS_LOG(2, "  fwrite returned %lld\n", (long long int) ret);
			return ret;
		}
	}

	ret = default_fwrite(ptr, size, nmemb, fp);
	return ret;	
}


int MEMFS_HOOK(fprintf)(FILE *stream, const char *format, ...){
//int fprintf(FILE *stream, const char *format, ...){
    int ret;

    init_wrappers();
    DEBUG_WRAPPER(fprintf)
    if(ski_memfs_enabled){

        memfs_file *f = memfs_get_file(stream);
        if(f){

			char fprintf_buff[1024];
			int fprintf_buff_size = 1024;

	        va_list ap;
	        va_start (ap, format);  

			ret = vsnprintf(fprintf_buff, fprintf_buff_size, format, ap);		
    	    va_end (ap);
			
			assert(ret<fprintf_buff_size);

			int fwrite_ret = fwrite(fprintf_buff, fprintf_buff_size, 1, stream);
			assert(fwrite_ret == 1);

/*
            int update_off = 1;
            START_TEST(size * nmemb);

            size_t bytes = memfs_write((char*)ptr, size, nmemb, f->off, f, update_off);
            ret = bytes / size;

            END_TEST(default_fwrite(ptr_original, size, nmemb, f->file), ret, 0);
*/
            MEMFS_LOG(2, "  fprintf wrote %lld\n", (long long int) ret);
            return ret;
        }
    }


	va_list ap;

	va_start(ap, format);
	ret = vfprintf(stream, format, ap);
	va_end(ap);

    return ret;
}


void MEMFS_HOOK(clearerr)(FILE *stream){
	
	init_wrappers();
	DEBUG_WRAPPER(clearerr)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if (f){
			assert(0);
		}
		//default_clearerr(stream);
	}
	default_clearerr(stream);
	return;
}

int MEMFS_HOOK(feof)(FILE *stream){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(feof)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if (f){
			assert(0);
		}
	}

	ret = default_feof(stream);
	return ret;
}

int MEMFS_HOOK(ferror)(FILE *stream){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(ferror)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if (f){
			assert(0);
		}
	}
	ret = default_ferror(stream);
	return ret;
}

int MEMFS_HOOK(fileno)(FILE *stream){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(fileno)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if (f){
			return f->id + MEMFS_FD_START;
			//return f->id + MEMFS_FD_START;
		}
	}
	ret = default_fileno(stream);
	return ret;
}



int MEMFS_HOOK(fseek)(FILE *stream, long offset, int whence){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(fseek)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if(f){
			switch(whence){
				case SEEK_SET:
					f->off = offset;
					break;
				case SEEK_CUR:
					f->off += offset;
					break;
				case SEEK_END:
					f->off = f->sup->length + offset; 
					break;
				default:
					assert(0);
			}
			assert(f->off >= 0);
			assert(f->off <= f->sup->length);
			ret = 0; // Sucess

			if(ski_memfs_test_mode_enabled){
				int ret_original = default_fseek(f->file, offset, whence);
				assert(ret_original == ret);
			}

		}else{
			ret = default_fseek(stream, offset, whence);
		}
		return ret;
	}

	ret = default_fseek(stream, offset, whence);
	return ret;
}

long MEMFS_HOOK(ftell)(FILE *stream){
	long ret;

	init_wrappers();
	DEBUG_WRAPPER(ftell)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if(f){
			ret = f->off;

			if(ski_memfs_test_mode_enabled){
				long ret_original = default_ftell(f->file);
				assert(ret == ret_original);
			}
		}else{
			ret = default_ftell(stream);
		}
		return ret;
	}

	ret = default_ftell(stream);
	return ret;
}

void MEMFS_HOOK(rewind)(FILE *stream){
	init_wrappers();
	DEBUG_WRAPPER(rewind)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if(f){
			f->off = 0;
			if(ski_memfs_test_mode_enabled){
				default_rewind(f->file);
		    }
		}else{
			default_rewind(stream);
		}
	}else{
		default_rewind(stream);
		
	}
}

int MEMFS_HOOK(fgetpos)(FILE *stream, fpos_t *pos){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(fgetpos)

	assert(0); // XXX: NYI

	ret = default_fgetpos(stream, pos);
	return ret;
}

int MEMFS_HOOK(fsetpos)(FILE *stream, __const fpos_t *pos){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(fsetpos)

	assert(0); // XXX: NYI

	ret = default_fsetpos(stream, (fpos_t*) pos);
	return ret;
}


/*==================================================================
                        FILE HOOK FUNCTIONS
==================================================================*/

/*int open(const char *pathname, int flags){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(open)

	ret = default_open(pathname, flags, 0);
	return ret;
}*/
int MEMFS_HOOK(open) (__const char *pathname, int flags, ...){
	int fd;

	init_wrappers();
	DEBUG_WRAPPER(open)

	MEMFS_LOG(2, "open file %s\n", pathname);

	if(flags & O_CREAT){
		// Get the mode argument
		// From the man: "mode specifies the permissions to use in case a new file is created.  This argument must be supplied when 
        //  O_CREAT is specified in flags; if O_CREAT is not specified, then mode is ignored."
		va_list ap;
		va_start (ap, flags);         
		mode_t mode = va_arg (ap, mode_t);    
		fd = default_open_mode(pathname, flags, mode);
		va_end (ap);
	}else{
		fd = default_open(pathname, flags);
	}

	if(fd == -1){
		MEMFS_LOG(0, "WARNING: open file %s returned -1\n", (char*)pathname);
	}

	if((!ski_memfs_enabled) || (!memfs_intercept_filename((char *)pathname))){
		MEMFS_LOG(2, "open file %s (%d) -> ignoring\n", (char*)pathname, fd);
		memfs_ignorelist_add(pathname);
		memfs_dump_all();
		return fd;
	}

    if(fd){
        memfs_file *f = memfs_open((char *)pathname, fd, 0);
		if(flags & O_APPEND){
			f->off = f->sup->length;
		}
        fd = f->id + MEMFS_FD_START;
		MEMFS_LOG(2, "open file %s (%d) -> intercepting\n", (char*)pathname, fd);
    }else{
        MEMFS_LOG(2, "open failed to open the file (%s)\n", pathname);
        fd = 0;
    }

	memfs_dump_all();
	return fd;
}
int MEMFS_HOOK(creat)(const char *pathname, mode_t mode){
	int ret;

	//  From the man: creat() is equivalent to open() with flags equal to O_CREAT|O_WRONLY|O_TRUNC.

	init_wrappers();
	DEBUG_WRAPPER(creat)

	if(ski_memfs_enabled){
		int flags = O_CREAT|O_WRONLY|O_TRUNC;
		ret = open(pathname, flags, mode);
	}else{
		ret = default_creat(pathname, mode);
	}

	return ret;
}
int MEMFS_HOOK(close)(int fd){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(close)

	MEMFS_LOG(2, "fclose file (%d)\n", fd);

	if(ski_memfs_enabled){
	    memfs_file *f = memfs_get_fd(fd);
		if(f){
	        ret = memfs_close(f);
		    return ret;
	    }
	}
	// XXX: Leak, we're leaving it open
	//ret = default_close(fd);
	ret = 0;	
	return ret;
}

ssize_t read(int fd, void *ptr, size_t count){
//ssize_t MEMFS_HOOK(read)(int fd, void *ptr, size_t count){
	ssize_t ret;

	init_wrappers();
	DEBUG_WRAPPER(read)

    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_fd(fd);
	    if(f){
			int update_off = 1;

			START_TEST(1 * count);

			size_t bytes = memfs_read((char*)ptr, 1, count, f->off, f, update_off);
			ret = bytes;

			END_TEST(default_read(f->fd, ptr_original, count), ret, (bytes>0)?(1*bytes):0);

			return ret;
		}
	}

	ret = default_read(fd, ptr, count);
	return ret;
}
ssize_t MEMFS_HOOK(pread)(int fd, void *ptr, size_t count, off_t offset){
	ssize_t ret;

	init_wrappers();
	DEBUG_WRAPPER(pread)

    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_fd(fd);
		if(f){
			int update_off = 0;
			
			START_TEST(1 * count);

			size_t bytes = memfs_read((char*)ptr, 1, count, offset, f, update_off);
			ret = bytes;

			END_TEST(default_pread(f->fd, ptr_original, count, offset), ret,  (bytes>0)?(1*bytes):0);

			return ret;
		}
	}

	ret = default_pread(fd, ptr, count, offset);
	return ret;
}

//ssize_t MEMFS_HOOK(write)(int fd, const void *ptr, size_t count){
ssize_t write(int fd, const void *ptr, size_t count){
	ssize_t ret;

	init_wrappers();
	DEBUG_WRAPPER(write)

    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_fd(fd);
		if(f){
			int update_off = 1;
			
			START_TEST(1 * count);

			size_t bytes = memfs_write((char*)ptr_original, 1, count, f->off, f, update_off);
			ret = bytes;

			END_TEST(default_write(f->fd, ptr_original, count), ret, 0);

			return ret;
		}
	}

	ret = default_write(fd, ptr, count);
	return ret;
}

ssize_t MEMFS_HOOK(pwrite)(int fd, const void *ptr, size_t count, off_t offset){
	ssize_t ret;

	init_wrappers();
	DEBUG_WRAPPER(pwrite)

    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_fd(fd);
		if(f){
			int update_off = 0;

			START_TEST(1 * count);

			size_t bytes = memfs_write((char*)ptr_original, 1, count, offset, f, update_off);
			ret = bytes;

			END_TEST(default_pwrite(f->fd, ptr_original, count, offset), ret, 0);
			
			return ret;
		}
	}

	ret = default_pwrite(fd, ptr, count, offset);
	return ret;
}


int MEMFS_HOOK(truncate)(const char *path, off_t length){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(truncate)

	MEMFS_LOG(2, "NYI\n");	
	assert(0);

	ret = default_truncate(path, length);
	return ret;
}

int MEMFS_HOOK(ftruncate)(int fd, off_t length){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(ftruncate)

    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_fd(fd);
		if(f){
			// We're supposed to leave the offset untouched
			if(length > f->sup->length){
				// Expansion 
				assert(0);
				// XXX: Not yet tested
				int update_off = 0;
				char *buf = 0;
				size_t count = length - f->sup->length;
				memfs_write(buf, 1, count, f->sup->length, f, update_off);
			}else{
				// Trimming (or equal)
				f->sup->length = length;
				if(ski_memfs_test_mode_enabled){
					ret = default_ftruncate(f->fd, length);
					assert(ret == 0);
				}
			}
			ret = 0; // Success
			return ret;
		}
	}

	ret = default_ftruncate(fd, length);
	return ret;
}

// Finish the rest

off_t MEMFS_HOOK(lseek)(int fd, off_t offset, int whence){
	off_t ret;

	init_wrappers();
	DEBUG_WRAPPER(lseek)

    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_fd(fd);
		if(f){
			MEMFS_LOG(2, "lseek (fd: %d offset: %lld whence: %d f->off: %lld f->length: %lld)\n", fd, (long long int) offset, whence, (long long int) f->off, (long long int) f->sup->length);
			switch(whence){
				case SEEK_SET:
					f->off = offset;
					break;
				case SEEK_CUR:
					f->off += offset;
					break;
				case SEEK_END:
					f->off = f->sup->length + offset;
					break;
				default:
					assert(0);
			}
			assert(f->off >= 0);
			assert(f->off <= f->sup->length);

			MEMFS_LOG(2, "lseek (fd: %d offset: %lld whence: %d f->off: %lld f->length: %lld)\n", fd, (long long int)  offset, whence, (long long int)  f->off, (long long int) f->sup->length);

			ret = f->off; // Sucess

			if(ski_memfs_test_mode_enabled){
				off_t ret_original = default_lseek(f->fd, offset, whence);
				assert(ret_original == ret);
			}
			return ret;
		}
	}

	ret = default_lseek(fd, offset, whence);
	return ret;
}

loff_t MEMFS_HOOK(llseek)(int fd, loff_t offset, int whence){
	off_t ret;

	init_wrappers();
	DEBUG_WRAPPER(llseek)

	// NYI: Function does not show up on man?!
	assert(0);

	ret = default_llseek(fd, offset, whence);
	return ret;
}

/*int _llseek(unsigned int fd, unsigned long offset_high, unsigned long offset_low, loff_t *result, unsigned int whence){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(_llseek)

	ret = default__llseek(fd, offset_high, offset_low, result, whence);
	return ret;
}*/
/*off64_t lseek64(int fd, off64_t offset, int whence){
	off64_t ret;

	init_wrappers();
	DEBUG_WRAPPER(lseek64)

	ret = default_lseek64(fd, offset, whence);
	return ret;
}
*/
int MEMFS_HOOK(select)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(select)

	int i;
	for(i=0; i<files_n ; i++){
		memfs_file *f = &files[i];
		int r = readfds ? FD_ISSET(f->fd, readfds) : 0;	
		int w = writefds ? FD_ISSET(f->fd, writefds) : 0;	
		int e = exceptfds ? FD_ISSET(f->fd, exceptfds): 0;
		//MEMFS_LOG(2, "select: checking if id=%d fd=%d closed=%d is included (%d %d %d)\n", f->id, f->fd, f->closed, r, w, e);	
		if((f->closed==0) && (r||w||e)){
			assert(0);
		}
		int re = readfds ? FD_ISSET(f->id + MEMFS_FD_START, readfds) : 0;	
		int we = writefds ? FD_ISSET(f->id + MEMFS_FD_START, writefds) : 0;	
		int ee = exceptfds ? FD_ISSET(f->id + MEMFS_FD_START, exceptfds): 0;
		//MEMFS_LOG(2, "select: checking if id=%d fd=%d closed=%d is included (%d %d %d)\n", f->id, f->fd, f->closed, r, w, e);	
		if((f->closed==0) && (re||we||ee)){
			assert(0);
		}
	}

	ret = default_select(nfds, readfds, writefds, exceptfds, timeout);
	return ret;
}
int MEMFS_HOOK(pselect)(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timespec *timeout, const sigset_t *sigmask){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(pselect)

	int i;
	for(i=0; i<files_n ; i++){
		memfs_file *f = &files[i];
		int r = FD_ISSET(f->fd, readfds);	
		int w = FD_ISSET(f->fd, writefds);	
		int e = FD_ISSET(f->fd, exceptfds);
		//MEMFS_LOG(2, "pselect: checking if id=%d fd=%d closed=%d is included (%d %d %d)\n", f->id, f->fd, f->closed, r, w, e);	
		if((f->closed==0) && (r||w||e)){
			assert(0);
		}
		int re = readfds ? FD_ISSET(f->id + MEMFS_FD_START, readfds) : 0;	
		int we = writefds ? FD_ISSET(f->id + MEMFS_FD_START, writefds) : 0;	
		int ee = exceptfds ? FD_ISSET(f->id + MEMFS_FD_START, exceptfds): 0;
		//MEMFS_LOG(2, "select: checking if id=%d fd=%d closed=%d is included (%d %d %d)\n", f->id, f->fd, f->closed, r, w, e);	
		if((f->closed==0) && (re||we||ee)){
			assert(0);
		}
	}

	ret = default_pselect(nfds, readfds, writefds, exceptfds, timeout, sigmask);
	return ret;
}

#if _FILE_OFFSET_BITS != 64
	#error "_FILE_OFFSET_BITS!=64 -- We're assuming that stat translates to SYS_stat64 (...)"
#endif

#if __x86_64 != 1
	#error "Not compliling for 64bits"
#endif

/*
	   struct stat {
		   dev_t     st_dev;     // ID of device containing file 
		   ino_t     st_ino;     // inode number 
		   mode_t    st_mode;    // protection 
		   nlink_t   st_nlink;   // number of hard links 
		   uid_t     st_uid;     // user ID of owner 
		   gid_t     st_gid;     // group ID of owner 
		   dev_t     st_rdev;    // device ID (if special file) 
		   off_t     st_size;    // total size, in bytes 
		   blksize_t st_blksize; // blocksize for file system I/O 
		   blkcnt_t  st_blocks;  // number of 512B blocks allocated 
		   time_t    st_atime;   // time of last access 
		   time_t    st_mtime;   // time of last modification 
		   time_t    st_ctime;   // time of last status change
	   };
*/
int MEMFS_HOOK(stat)(const char *path, struct stat *buf){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(stat)

	ret = syscall(__NR_stat, path, buf);
	//ret = default_stat(path, buf);
    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_filename(path);
		if(f){
			assert(ret == 0);
			if(ski_memfs_test_mode_enabled){
				assert(buf->st_size == f->sup->length);
			}

			buf->st_size = f->sup->length;
		}
	}
	return ret;
}

int MEMFS_HOOK(fstat)(int fd, struct stat *buf){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(fstat)

	//ret = default_fstat(fd, buf);

    if(ski_memfs_enabled){
		memfs_file *f = memfs_get_fd(fd);
		if(f){
			int patched_fd = f->fd;
			ret = syscall(__NR_fstat, patched_fd, buf);
			assert(ret==0);
			if(ski_memfs_test_mode_enabled){
				assert(buf->st_size == f->sup->length);
			}
			buf->st_size = f->sup->length;
			return ret;
		}
	}

	ret = syscall(__NR_fstat, fd, buf);
	
	MEMFS_LOG(2, "MEMFS: end fstat\n");
	return ret;
}

int statfs64(const char *path, struct statfs64 *buf){
	MEMFS_LOG(2, "statfs64\n");

	default_statfs64(path, buf);

	assert(0);
}

int __lxstat64(int __ver, __const char *__filename, struct stat64 *__stat_buf){
	MEMFS_LOG(2, "__lxstat64\n");

	default___lxstat64(__ver, __filename, __stat_buf);

	assert(0);
}

//int MEMFS_HOOK(lstat)(const char *path, struct stat *buf){
int lstat(const char *path, struct stat *buf){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(lstat)

	// NYI
	assert(0);

	ret = syscall(__NR_lstat, path, buf);
	//ret = default_lstat(path, buf);

	return ret;
}


#define MEMFS_FCNTL_PARSE_START(type)	\
		{								\
			va_list ap;					\
			va_start (ap, cmd);			\
			type arg3 = va_arg (ap, type);	

#define MEMFS_FCNTL_PARSE_END()	\
			va_end(ap);	\
		}

int MEMFS_HOOK(fcntl)(int fd, int cmd, ... /* arg */ ){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(fcntl)


	switch(cmd){
		case F_DUPFD:
			MEMFS_FCNTL_PARSE_START(long);
			ret = default_fcntl(fd, cmd, arg3);
			MEMFS_FCNTL_PARSE_END();
			break;
		case F_DUPFD_CLOEXEC:
			MEMFS_FCNTL_PARSE_START(long);
			ret = default_fcntl(fd, cmd, arg3);
			MEMFS_FCNTL_PARSE_END();
			break;
		case F_GETFD:
			ret = default_fcntl(fd, cmd);
			break;
		case F_SETFD:
			MEMFS_FCNTL_PARSE_START(long);
			ret = default_fcntl(fd, cmd, arg3);
			MEMFS_FCNTL_PARSE_END();
			break;
		case F_GETFL:
			ret = default_fcntl(fd, cmd);
			break;
		case F_SETFL:
			MEMFS_FCNTL_PARSE_START(long);
			ret = default_fcntl(fd, cmd, arg3);
			MEMFS_FCNTL_PARSE_END();
			break;
		default:
			assert(0);
			break;
	}

	return ret;
}

// XXX: Add test mode support for this one
char *MEMFS_HOOK(fgets)(char *s, int size, FILE *stream){
	char* ret;
	
	init_wrappers();
	DEBUG_WRAPPER(fgets)

	if(ski_memfs_enabled){
		memfs_file *f = memfs_get_file(stream);
		if(f){
			char str[1024*4];
			assert(size<sizeof(str));

			size_t offset = f->off;
			int update_off = 0;
			size_t bytes = memfs_read((char*)str, 1, size-1, offset, f, update_off);
			assert(bytes<size);
			str[bytes] = 0;
			char *nl = strchr(str,'\n');
			if(nl){
				// End string after the newline if it is found
				nl[1] = 0;
			}
			
			size_t len = strlen(str);
			f->off+=len;
			strcpy(s, str);
			if(len){
				MEMFS_LOG(2, "finish fgets (ret %p, ret len: %lld)\n", s, (long long int) strlen(s));
				return s;
			}else{
				MEMFS_LOG(2, "finish fgets (ret %d, ret len: %d)\n", 0, 0);
				return 0;
			}
		}
	}

	ret = default_fgets(s, size, stream);
	MEMFS_LOG(2, "finish fgets (ret %p, ret len: %lld)\n", ret, ret?(long long int)strlen(ret):0L);
	return ret;
}


// NYI for filtered files/streams
int MEMFS_HOOK(dup)(int oldfd){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(dup)

	if(ski_memfs_enabled){
		int f_index = 0;

		for(f_index = 0;f_index < files_n; f_index++){
			memfs_file* f = &files[f_index];
			if(f->fd == oldfd){
				assert(0);
			}
		}
	}

	ret = default_dup(oldfd);
	return ret;
}
int MEMFS_HOOK(dup2)(int oldfd, int newfd){
	int ret;

	init_wrappers();
	DEBUG_WRAPPER(dup2)

	if(ski_memfs_enabled){
		int f_index = 0;

		// NYI
		for(f_index = 0;f_index < files_n; f_index++){
			memfs_file* f = &files[f_index];
			if((f->fd == oldfd) || (f->fd == newfd)){
				assert(0);
			}
		}
	}

	ret = default_dup2(oldfd, newfd);
	return ret;

}
int MEMFS_HOOK(dup3)(int oldfd, int newfd, int flags){
	assert(0); // XXX: NYI
}
/*int openat(int dirfd, const char *pathname, int flags){
	assert(0); // XXX: NYI
}*/
int MEMFS_HOOK(openat) (int __fd, __const char *__file, int __oflag, ...) {
//int openat(int dirfd, const char *pathname, int flags, mode_t mode){
	assert(0); // XXX: NYI
}
ssize_t MEMFS_HOOK(readv)(int fd, const struct iovec *iov, int iovcnt){
	assert(0); // XXX: NYI
}
ssize_t MEMFS_HOOK(writev)(int fd, const struct iovec *iov, int iovcnt){
	assert(0); // XXX: NYI
}

/*
int chown(const char *path, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int lchown(const char *path, uid_t owner, gid_t group);

int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);

int fcntl(int fd, int cmd, ...  );

int link(const char *oldpath, const char *newpath);
int mknod(const char *pathname, mode_t mode, dev_t dev);
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int mount(const char *source, const char *target,const char *filesystemtype, unsigned long mountflags,const void *data);

int socket(int domain, int type, int protocol);
int unlink(const char *pathname);
*/



// Use glib interception (i.e. don't use source instrumetation for this)

int timer_create(clockid_t clockid, struct sigevent *evp,
                        timer_t *timerid){
    int ret;

    init_wrappers();
    DEBUG_WRAPPER(timer_create)

	ret = default_timer_create(clockid, evp, timerid);

	//return -1;
	return ret;
}

int timer_settime(timer_t timerid, int flags,
                         const struct itimerspec *new_value,
                         struct itimerspec * old_value)
{
    int ret;

    init_wrappers();
    DEBUG_WRAPPER(timer_settime)

	default_timer_settime(timerid, flags, new_value, old_value);

	return ret;
}

int timer_gettime(timer_t timerid, struct itimerspec *curr_value)
{
    int ret;

    init_wrappers();
    DEBUG_WRAPPER(timer_gettime)

	default_timer_gettime(timerid, curr_value);
	return ret;
}


/*==================================================================
                        TESTING FUNCTIONS
==================================================================*/

#else // if not MEMFS_DISABLED

#warning "Compling without memfs"

#endif // MEMFS_DISABLED

#ifdef MEMFS_MAIN

void test_read_write(FILE *fd){
	char buffer[1024*64];
	int max = 1024;
	int i;

	fseek(fd, -2, SEEK_END);

	buffer[0] = 0;

	for(i=0;i<max;i++){
		sprintf(buffer + strlen(buffer), "%04d\n", i);
	}

	int batch = 4;
	for(i=0;i<strlen(buffer);i+=batch){
		fwrite(buffer+i, batch, 1, fd);
	}

	rewind(fd);

	int res;
	i = 0;
	do{
		res = fread(buffer+i, 1, 1, fd);
		if(res)
			fprintf(stdout, "%c", *((char*) buffer+i));
		i++;
	}while(res>0);

	// TODO::w

}




int main(int argc, char** argv){
	printf("Memfs\n");

	FILE *f = fopen("test.txt","r+");
	printf("f = %p\n", f);

	char buf[256];
	size_t s;
	//s = fwrite(buf, 100, 1, f);
	//printf("fwrite results %lld\n", (long long )s);

	s = fread(buf, 100, 1, f);
	printf("fread results %lld\n", (long long) s);

	test_read_write(f);

	fclose(f);
	return 0;
}
#endif // MEMFS_MAIN

