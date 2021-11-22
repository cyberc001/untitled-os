#include "fs.h"

#include "ext2.h"
int fs_scan(file_system* fs, ata_drive* drive)
{
	fs->drive = drive;
	if(fs_ext2_gfs_detect(fs))
	{ fs_ext2_gfs_init(fs); return 1; }

	fs->name = "none";
	return 0;
}

//-----------------------
// Path parsing functions
//-----------------------

char* fs_next_file(char** cur)
{
	char* ret = *cur;
	if(*ret == '\0')
		return NULL;

	for(; **cur != '/' && **cur != '\0'; ++(*cur))
		;
	int was_null = **cur == '\0';
	**cur = '\0';
	if(!was_null)
		++(*cur);

	return ret;
}
