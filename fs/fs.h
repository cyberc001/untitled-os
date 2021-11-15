#ifndef FS_H
#define FS_H

#include <stddef.h>

#include "../dev/ata.h"

/* Generic file system structure and functions.
*  Functions for initializing a generic file system should be included for each
*  separate file system in their header/source file.
*/
typedef struct _file_system
{
	ata_drive* drive;
	size_t fd_size;		// size of the file handler structure
	void* gdata;		// generic data, depending on underlying file system
				// (ex. superblock data for ext2)
				// should be dynamically allocated, and kfree() is called
				// on deinitialization if not NULL (FIXME)


	/* All file operations use a generic void pointer file handler.
	*  Structure and usage of each file system's handler type varies.
	*  File descriptors in terms of write/read protection (ex. for multi-processing)
	*  are handled by other modules (closer to system calls implementation).
	*  Memory for file descriptor should be allocated before calling open(),
	*  and after calling close() (file systems don't manage memory).
	*/

	int (*create)(struct _file_system* fs, const char* path, int flags);
	int (*unlink)(struct _file_system* fs, const char* path);
	int (*rename)(struct _file_system* fs, const char* old, const char* _new);

	int (*open)(struct _file_system* fs, void* fd, const char* path, int flags);
	void (*close)(struct _file_system* fs, void* fd);

	long (*lseek)(struct _file_system* fs, void* fd, long offset, int whence);
	size_t (*read)(struct _file_system* fs, void* fd, void* buf, size_t count);
	size_t (*write)(struct _file_system* fs, void* fd, const void* buf, size_t count);
} file_system;

// function flags:

#define FS_CREATE_DIR		0x01	// creates directory instead of a file.

#define FS_OPEN_READ		0x01
#define FS_OPEN_WRITE		0x02
#define FS_OPEN_CREATE		0x04	// creates file for write if it does not exist.
#define FS_OPEN_RECREATE	0x08	// in conjunction with FS_OPEN_WRITE removes all contents
					// of the file written before opening it if it exists
#define FS_OPEN_ENDPTR		0x10	// set pointer to the end of file (ex. for append mode)


// error codes:

#define FS_ERR_DOESNT_EXIST		-1	// one of the components of file path does not exist
						// (so it is expected in unlink() as well)
#define FS_ERR_DIRS_UNSUPPORTED		-2	// directories are not supported by this operation
						// (usually by open(), since it makes little sense)
#define FS_ERR_SEEK_UNSUPPORTED		-3	// either file system or file does not support
						// seeking (like standard I/O files)

//-----------------------
// Path parsing functions
//-----------------------

/* Return value:
*  pointer to next file name,
*  or NULL if end of the pathname was reached.
*/
char* fs_next_file(char** cur);

#endif
