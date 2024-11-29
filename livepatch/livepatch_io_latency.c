#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/livepatch.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/kallsyms.h>

static char *dev_name = "/dev/sda";
static long dev_inode;
static long read_time = 0;
static long write_time = 0;

module_param(read_time, long, 0644);
MODULE_PARM_DESC(read_time, "read_latency_time (default=0ms)");
module_param(write_time, long, 0644);
MODULE_PARM_DESC(write_time, "write_latency_time (default=0ms)");
module_param(dev_name, charp, 0644);
MODULE_PARM_DESC(dev_name, "dev_name (default=/dev/sda)");

static ssize_t (*new_sync_read)(struct file *filp, char __user *buf, size_t len,
			 loff_t *ppos);
static ssize_t (*new_sync_write)(struct file *filp, const char __user *buf,
				 size_t len, loff_t *ppos);

static long inode_by_sys_stat(void)
{
	struct path path;
	int error;
	struct inode *inode;
	error = kern_path(dev_name,LOOKUP_FOLLOW,&path);
	if (error){
		return error;
	}
	inode = path.dentry->d_inode;
	path_put(&path);
	return inode->i_ino;
}

static void check_read_dev(struct file *file)
{
	struct inode *inode = file->f_inode;
	dev_t dev = inode->i_sb->s_dev;
	long inode_number = inode->i_ino;
	if (inode_number == dev_inode) {
		pr_debug("The file inode number is %lu\n", inode_number);
		if (read_time != 0){
			msleep(read_time);
		}
	}

	if (MAJOR(dev) == 8 && MINOR(dev) == 0){
		pr_info("The file inode to MAJOR 8 , MINOR 0");
	}
}

static void check_write_dev(struct file *file)
{
	struct inode *inode = file->f_inode;
	long inode_number = inode->i_ino;
	if (inode_number == dev_inode) {
		pr_debug("The file inode number is %lu\n", inode_number);
		if (write_time != 0) {
			msleep(write_time);
		}
	}
}

ssize_t livepatch__vfs_read(struct file *file, char __user *buf, size_t count,
		   loff_t *pos)
{
	check_read_dev(file);

	if (file->f_op->read)
		return file->f_op->read(file, buf, count, pos);
	else if (file->f_op->read_iter)
		return new_sync_read(file, buf, count, pos);
	else
		return -EINVAL;
}

ssize_t livepatch__vfs_write(struct file *file, const char __user *p,
			     size_t count, loff_t *pos)
{
	check_write_dev(file);

	if (file->f_op->write)
		return file->f_op->write(file, p, count, pos);
	else if (file->f_op->write_iter)
		return new_sync_write(file, p, count, pos);
	else
		return -EINVAL;
}

static struct klp_func funcs[] = {
    {
        .old_name = "__vfs_read",
		.new_func = livepatch__vfs_read,
    }, 
	{ 
		.old_name = "__vfs_write",
		.new_func = livepatch__vfs_write,
	}, { }
};

static struct klp_object objs[] = {
    {
        .funcs = funcs,
    }, { }
};

static struct klp_patch patch = {
    .mod = THIS_MODULE,
    .objs = objs,
};

static int livepatch_init(void)
{
	new_sync_read = (void *)kallsyms_lookup_name("new_sync_read");
	new_sync_write = (void *)kallsyms_lookup_name("new_sync_write");
	dev_inode = inode_by_sys_stat();
    return klp_enable_patch(&patch);
}

static void livepatch_exit(void)
{
}

module_init(livepatch_init);
module_exit(livepatch_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");