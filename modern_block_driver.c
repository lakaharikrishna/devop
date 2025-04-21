#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/bio.h>

#define SECTOR_SIZE 512
#define DEVICE_NAME "myblockdev"
#define DISK_NAME "myblockdisk"
#define NSECTORS 1024  // Device size = 512KB

static struct my_block_dev {
    struct gendisk *gd;
    spinlock_t lock;
    struct request_queue *queue;
    u8 *data;
} dev;

static void myblock_request(struct request_queue *q)
{
    struct request *req;
    
    req = blk_mq_rq_from_pdu(bio->bi_private);
    while ((req = blk_fetch_request(q)) != NULL) {
        // Process the request here
        __blk_end_request_all(req, 0);
    }
}

static int myblock_open(struct block_device *bdev, fmode_t mode)
{
    printk(KERN_INFO "myblock device opened\n");
    return 0;
}

static void myblock_release(struct gendisk *gd, fmode_t mode)
{
    printk(KERN_INFO "myblock device closed\n");
}

static const struct block_device_operations myblock_fops = {
    .owner = THIS_MODULE,
    .open = myblock_open,
    .release = myblock_release,
};

static int __init myblock_init(void)
{
    // Allocate memory for our device
    dev.data = vmalloc(NSECTORS * SECTOR_SIZE);
    if (!dev.data)
        return -ENOMEM;
    
    // Initialize spinlock
    spin_lock_init(&dev.lock);
    
    // Create request queue  with only one queue
    dev.queue = blk_mq_init_sq_queue(&dev.tag_set, &myblock_mq_ops, 128, BLK_MQ_F_SHOULD_MERGE);
    if (IS_ERR(dev.queue)) {
        vfree(dev.data);
        return PTR_ERR(dev.queue);
    }
    blk_queue_logical_block_size(dev.queue, SECTOR_SIZE);//set logical block size for the queue
    
    // Allocate gendisk structure
    dev.gd = alloc_disk(1);
    if (!dev.gd) {
        blk_cleanup_queue(dev.queue);
        vfree(dev.data);
        return -ENOMEM;
    }
    
    // Set up the gendisk structure
    dev.gd->major = 0;  // Dynamic major
    dev.gd->first_minor = 0;
    dev.gd->fops = &myblock_fops;
    dev.gd->queue = dev.queue;
    dev.gd->private_data = &dev;
    snprintf(dev.gd->disk_name, 32, DISK_NAME);
    set_capacity(dev.gd, NSECTORS);
    
    // Add the disk
    add_disk(dev.gd);
    
    printk(KERN_INFO "myblock device initialized\n");
    return 0;
}

static void __exit myblock_exit(void)
{
    del_gendisk(dev.gd);
    put_disk(dev.gd);
    blk_cleanup_queue(dev.queue);
    vfree(dev.data);
    printk(KERN_INFO "myblock device removed\n");
}

module_init(myblock_init);
module_exit(myblock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Simple block device driver");
