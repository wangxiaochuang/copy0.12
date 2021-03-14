#ifndef _ISO_FS_I
#define _ISO_FS_I

struct iso_inode_info {
	unsigned int i_first_extent;
	unsigned int i_backlink;
	unsigned char i_file_format;
};

#endif
