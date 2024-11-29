#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/livepatch.h>
#include <linux/kallsyms.h>
#include <linux/bio.h>
#include <linux/blk-cgroup.h>
#include <linux/blkdev.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/delay.h>

/*
 * Flag that makes the machine dump writes/reads and block dirtyings.
 */
int block_dump;
static struct workqueue_struct *blkcg_punt_bio_wq;
struct block_device *bdev;
dev_t chek_dev;
dev_t dev;
int bio_major;
int bio_minor;
int check_marjor;
int check_minor;
// bool (*__blkcg_punt_bio_submit)(struct bio *bio);
static char *disk_path = "";
static int latency = 0;
static bool fault = 0;
static int start_sector=0;
static int end_sector=0;

// module_param(read_time, long, 0644);
// MODULE_PARM_DESC(read_time, "read_latency_time (default=0ms)");

void remove_newlines(char *str) {
    char *src = str;
    char *dst = str;
    
    while (*src) {
        if (*src != '\n') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0'; // Null-terminate the resulting string
}

static int param_set_callback(const char *val, const struct kernel_param *kp)
{	
	int ret;
	ret = param_set_charp(val, kp);
	pr_info("lookup block device for %s ,err: %d",val,ret);
	if (ret == 0){
		remove_newlines(disk_path);
		bdev = lookup_bdev(disk_path);
   	    if (IS_ERR(bdev)) {
        	pr_err("Failed to lookup block device for %s error: %ld\n", disk_path, PTR_ERR(bdev));
			check_marjor = 0;
			check_minor = 0;
			return ret;
    	}
		chek_dev = bdev->bd_dev;
		check_marjor = MAJOR(chek_dev);
		check_minor =  MINOR(chek_dev);
		pr_info("chaos lookup block device marjor:minor is %d:%d\n", check_marjor,check_minor);
	}
	return ret;
}

static inline void bio_fault_error(struct bio *bio)
{
	bio_set_flag(bio, BIO_QUIET);
	bio->bi_status = BLK_STS_AGAIN;
	bio_endio(bio);
}

bool __blkcg_punt_bio_submit(struct bio *bio)
{
	struct blkcg_gq *blkg = bio->bi_blkg;

	/* consume the flag first */
	bio->bi_opf &= ~REQ_CGROUP_PUNT;

	/* never bounce for the root cgroup */
	if (!blkg->parent)
		return false;

	spin_lock_bh(&blkg->async_bio_lock);
	bio_list_add(&blkg->async_bios, bio);
	spin_unlock_bh(&blkg->async_bio_lock);

	queue_work(blkcg_punt_bio_wq, &blkg->async_bio_work);
	return true;
}

blk_qc_t livepatch_submit_bio(struct bio *bio)
{
	dev = bio_dev(bio);
	bio_major = MAJOR(dev);
	bio_minor = MINOR(dev);
	if (bio_major == check_marjor && bio_minor == check_minor){
		pr_info("check bio");
		if (fault){
			bio_fault_error(bio);
			return BLK_QC_T_NONE;
		}
		if (end_sector > start_sector){
			sector_t s_start_sector = start_sector;
			sector_t s_end_sector = end_sector;
			sector_t bio_start = bio->bi_iter.bi_sector;
			sector_t bio_end = bio_end_sector(bio);
			if (bio_start >= s_start_sector && bio_end <= s_end_sector) {
				bio_fault_error(bio);
				return BLK_QC_T_NONE;
			}
		}
		if (latency){
			usleep_range(latency,10);
		}
	}
	
	if (blkcg_punt_bio_submit(bio))
		return BLK_QC_T_NONE;

	/*
	 * If it's a regular read/write or a barrier with data attached,
	 * go through the normal accounting stuff before submission.
	 */
	if (bio_has_data(bio)) {
		unsigned int count;

		if (unlikely(bio_op(bio) == REQ_OP_WRITE_SAME))
			count = queue_logical_block_size(bio->bi_disk->queue) >> 9;
		else
			count = bio_sectors(bio);

		if (op_is_write(bio_op(bio))) {
			count_vm_events(PGPGOUT, count);
		} else {
			task_io_account_read(bio->bi_iter.bi_size);
			count_vm_events(PGPGIN, count);
		}

		if (unlikely(block_dump)) {
			char b[BDEVNAME_SIZE];
			printk(KERN_DEBUG "%s(%d): %s block %Lu on %s (%u sectors)\n",
			current->comm, task_pid_nr(current),
				op_is_write(bio_op(bio)) ? "WRITE" : "READ",
				(unsigned long long)bio->bi_iter.bi_sector,
				bio_devname(bio, b), count);
		}
	}

	return generic_make_request(bio);
}

static struct klp_func funcs[] = {
    {
        .old_name = "submit_bio",
		.new_func = livepatch_submit_bio,
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
    blkcg_punt_bio_wq = (void *)kallsyms_lookup_name("blkcg_punt_bio_wq");
	// new_sync_write = (void *)kallsyms_lookup_name("new_sync_write");
	// dev_inode = inode_by_sys_stat();
    return klp_enable_patch(&patch);
}

static void livepatch_exit(void)
{
}
module_param_call(disk_path,param_set_callback,param_get_charp,&disk_path, S_IWUSR | S_IRUGO);
MODULE_PARM_DESC(disk_path, "chaos disk(default /dev/sda)");
module_param(latency,int,0644);
MODULE_PARM_DESC(latency, "disk latency time(default 0ms)");
module_param(fault,bool,0644);
MODULE_PARM_DESC(fault, "disk fault(default 0)");
module_param(start_sector, int , 0644);
MODULE_PARM_DESC(start_sector, "disk start_sector(default 0)");
module_param(end_sector, int , 0644);
MODULE_PARM_DESC(end_sector, "disk end_sector(default 0)");
module_init(livepatch_init);
module_exit(livepatch_exit);
MODULE_LICENSE("GPL");
MODULE_INFO(livepatch, "Y");