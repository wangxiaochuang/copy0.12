#include <linux/fs.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <linux/errno.h>

struct device_struct {
	const char * name;
	struct file_operations * fops;
};

static struct device_struct chrdevs[MAX_CHRDEV] = {
	{ NULL, NULL },
};

int register_chrdev(unsigned int major, const char * name, struct file_operations *fops) {
    if (major >= MAX_CHRDEV)
        return -EINVAL;
    if (chrdevs[major].fops)
        return -EBUSY;
    chrdevs[major].name = name;
    chrdevs[major].fops = fops;
    return 0;
}