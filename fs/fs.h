#ifndef FS_H
#define FS_H

#include <stddef.h>

#include "../dev/ata.h"

/* Directory entry structure for iterating though directory.
*  Should be allocated by dir_iter_next() caller, including name field.
*  If types are not supported, type field should always be FS_CREATE_TYPE_UNKNOWN.
*/
typedef struct{
	size_t name_max;
	char* name;

	unsigned int type; // uses FS_CREATE_TYPE flags
} file_system_dirent;

/* Generic file system structure and functions.
*  Functions for initializing a generic file system should be included for each
*  separate file system in their header/source file.
*/
typedef struct _file_system
{
	ata_drive* drive;

	const char* name;	// file system name, assigned by corresponding init() function

	size_t fd_size;		// size of file handler structure
	size_t dit_size;	// size of directory iterator structure

	void* gdata;		// generic data, depending on underlying file system
				// (ex. superblock data for ext2)
				// should be dynamically allocated, and kfree() is called
				// on deinitialization if not NULL


	/* - Most file operations use a generic void pointer file handler.
	*    Structure and usage of each file system's handler type varies.
	*
	*  - File descriptors in terms of write/read protection (ex. for multi-processing)
	*    are handled by other modules (closer to system calls implementation).
	*
	*  - Memory for file descriptor should be allocated before calling open(),
	*    and after calling close() (file systems don't manage memory).
	*
	*  - create(), unlink(), rename(), open() return 0 on success and other values
	*    (often negative) on error, described through generic error codes below.
	*
	*  - lseek() returns new position on success and negative values on error.
	*
	*  - read() and write() return 0 on error or zero length reads/writes, and
	*    amount of read/written bytes otherwise.
	*    These operations also automatically seek past read/written data chunks.
	*
	*  - get_size() returns 0 on error or on an empty file.
	*
	*  - dir_iter_start() returns a negative error value if iterating through directory
	*    is impossible (ex. it does not exist), 0 otherwise.
	*
	*  - dir_iter_next() returns 0 if there are no more entries in directory,
	*    1 otherwise, and write fetched directory entry in 'dent' field.
	*/

	int (*create)(struct _file_system* fs, const char* path, int type);
	int (*unlink)(struct _file_system* fs, const char* path);
	int (*rename)(struct _file_system* fs, const char* old, const char* _new);

	int (*open)(struct _file_system* fs, void* fd, const char* path, int flags);
	void (*close)(struct _file_system* fs, void* fd);
	size_t (*get_size)(struct _file_system* fs, void* fd);

	long (*seek)(struct _file_system* fs, void* fd, long offset, int whence);
	size_t (*read)(struct _file_system* fs, void* fd, void* buf, size_t count);
	size_t (*write)(struct _file_system* fs, void* fd, const void* buf, size_t count);

	int (*dir_iter_start)(struct _file_system* fs, void* iter, const char* path);
	int (*dir_iter_next)(struct _file_system* fs, void* iter, file_system_dirent* dent);
} file_system;


// function flags:

/* Following values before a newline are in fact possible file types.
*  open() implementations are free to not support any type except FILE and DIR.
*  In case of unknown type open() should return FS_ERR_TYPE_UNSUPPORTED.
*/
#define FS_CREATE_TYPE_UNKNOWN		0x00	// Invalid type, useful for utility functions.
#define FS_CREATE_TYPE_FILE		0x01
#define FS_CREATE_TYPE_DIR		0x02
#define FS_CREATE_TYPE_SYMLINK		0x03
#define FS_CREATE_TYPE_FIFO		0x04
#define FS_CREATE_TYPE_DEVCHAR		0x05
#define FS_CREATE_TYPE_DEVBLK		0x06
#define FS_CREATE_TYPE_UNIX_SOCK	0x07

#define FS_OPEN_READ		0x01
#define FS_OPEN_WRITE		0x02
#define FS_OPEN_CREATE		0x04	// Creates file for write if it does not exist.
#define FS_OPEN_RECREATE	0x08	// In conjunction with FS_OPEN_WRITE removes all contents
					// of the file written before opening it if it exists.
#define FS_OPEN_ENDPTR		0x10	// Set pointer to the end of file (ex. for append mode).

#define FS_SEEK_BEGIN		0x00
#define FS_SEEK_CUR		0x01
#define FS_SEEK_END		0x02


// error codes:

#define FS_ERR_DOESNT_EXIST		-1	// One of the components of file path does not exist
						// (so it is expected in unlink() as well).
#define FS_ERR_DIRS_UNSUPPORTED		-2	// Directories are not supported by this operation
						// (usually by open(), since it makes little sense).
#define FS_ERR_SEEK_UNSUPPORTED		-3	// Either file system or file does not support
						// seeking (like standard I/O files).
#define FS_ERR_INVALID_FILENAME		-4	// Operation does not support such file names
						// (ex. create() does not support empty file names).
#define FS_ERR_NO_SPACE			-5	// Not enough free space for operation.
#define FS_ERR_TYPE_UNSUPPORTED		-6	// The operation does not support specified type
						// (ex. create()).


/* Scans specified drive, trying to identify a valid file system.
*  Uses functions defined as "int fs_XX_gfs_detect(file_system*)",
*  where XX - certain file system name. All probed file systems headers
*  and source files are in fs directory.
*  When a valid file system is found, it is initialized by a function such as
*  "fs_XX_gfs_init(file_system*)", and 1 is returned. Otherwise, 0 is returned.
*/
int fs_scan(file_system* fs, ata_drive* drive);

//-----------------------
// Path parsing functions
//-----------------------

/* Return value:
*  pointer to next file name,
*  or NULL if end of the pathname was reached.
*/
char* fs_next_file(char** cur);

#endif
