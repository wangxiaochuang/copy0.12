#include <linux/fs.h>
#include <linux/string.h>
#include <linux/mm.h>

struct file * first_file;

unsigned long file_table_init(unsigned long start, unsigned long end) {
	first_file = NULL;
	return start;
}