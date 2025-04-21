#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/vmalloc.h>
#include <linux/blk-mq.h>
#include <linux/slab.h>
#include <linux/printk.h>

#define MY_SECTOR_SIZE 512
#define DISK_NAME "myblockdisk"
#define NSECTORS 1024
#define DEBUG_LEVEL KERN_EMERG

struct my_block_dev {
    struct gendisk *gd;
    struct request_queue *queue;
    struct blk_mq_tag_set tag_set;
    u8 *data;
    atomic_t initialized; // Atomic flag for initialization state
};

static struct my_block_dev *dev; // Changed to pointer

static blk_status_t myblock_queue_rq(struct blk_mq_hw_ctx *hctx,const struct blk_mq_queue_data *bd)
{
    struct request *req = bd->rq;
    struct bio *bio;
    blk_status_t err = BLK_STS_OK;

    if (!atomic_read(&dev->initialized)) {
        return BLK_STS_IOERR;
    }

    blk_mq_start_request(req);

    bio = req->bio;
    while (bio) {
        struct bio_vec bvec;
        struct bvec_iter iter;
        unsigned long offset = bio->bi_iter.bi_sector * MY_SECTOR_SIZE;
        
        if (offset + bio->bi_iter.bi_size > NSECTORS * MY_SECTOR_SIZE) {
            return BLK_STS_IOERR;
        }

        bio_for_each_segment(bvec, bio, iter) {
            void *dptr = kmap(bvec.bv_page) + bvec.bv_offset;
            
            if (req_op(req) == REQ_OP_READ)
                memcpy(dptr, dev->data + offset, bvec.bv_len);
            else if (req_op(req) == REQ_OP_WRITE)
                memcpy(dev->data + offset, dptr, bvec.bv_len);
            
            kunmap(bvec.bv_page);
            offset += bvec.bv_len;
        }
        bio = bio->bi_next;
    }

    blk_mq_end_request(req, err);
    return err;
}

static const struct blk_mq_ops myblock_mq_ops = {
    .queue_rq = myblock_queue_rq,
};

static int myblock_open(struct gendisk *gd, blk_mode_t mode)
{
    if (!atomic_read(&dev->initialized)) {
        return -ENODEV;
    }
    return 0;
}

static void myblock_release(struct gendisk *gd)
{
    // Empty for basic implementation
}

static const struct block_device_operations myblock_fops = {
    .owner = THIS_MODULE,
    .open = myblock_open,
    .release = myblock_release,
};

static int __init myblock_init(void)
{
    int ret = 0;
    
    printk(DEBUG_LEVEL "BDD: Initialization started\n");

    // Allocate device structure
    dev = kzalloc(sizeof(*dev), GFP_KERNEL);
    if (!dev) {
        printk(DEBUG_LEVEL "BDD: Failed to allocate device structure\n");
        return -ENOMEM;
    }

    // Initialize atomic flag
    atomic_set(&dev->initialized, 0);

    // Allocate device memory
    dev->data = vzalloc(NSECTORS * MY_SECTOR_SIZE);
    if (!dev->data) {
        printk(DEBUG_LEVEL "BDD: Memory allocation failed\n");
        ret = -ENOMEM;
        goto out_free_dev;
    }

    // Initialize tag set
    memset(&dev->tag_set, 0, sizeof(dev->tag_set));
    dev->tag_set.ops = &myblock_mq_ops;
    dev->tag_set.nr_hw_queues = 1;
    dev->tag_set.queue_depth = 128;
    dev->tag_set.numa_node = NUMA_NO_NODE;
    dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;

    ret = blk_mq_alloc_tag_set(&dev->tag_set);
    if (ret) {
        printk(DEBUG_LEVEL "BDD: Tag set allocation failed: %d\n", ret);
        goto out_free_mem;
    }

    // Create request queue
    dev->queue = blk_mq_init_queue(&dev->tag_set);
    if (IS_ERR(dev->queue)) {
        ret = PTR_ERR(dev->queue);
        printk(DEBUG_LEVEL "BDD: Queue initialization failed: %d\n", ret);
        goto out_free_tags;
    }
    blk_queue_logical_block_size(dev->queue, MY_SECTOR_SIZE);

    // Allocate disk structure
    dev->gd = blk_alloc_disk(NUMA_NO_NODE);
    if (!dev->gd) {
        printk(DEBUG_LEVEL "BDD: Disk allocation failed\n");
        ret = -ENOMEM;
        goto out_free_queue;
    }

    // Configure disk
    dev->gd->major = 0;  // Dynamic major
    dev->gd->first_minor = 0;
    dev->gd->fops = &myblock_fops;
    dev->gd->queue = dev->queue;
    dev->gd->private_data = dev;
    snprintf(dev->gd->disk_name, sizeof(dev->gd->disk_name), DISK_NAME);
    set_capacity(dev->gd, NSECTORS);

    // Add disk
    ret = add_disk(dev->gd);
    if (ret) {
        printk(DEBUG_LEVEL "BDD: Failed to add disk: %d\n", ret);
        goto out_free_disk;
    }

    atomic_set(&dev->initialized, 1);
    printk(DEBUG_LEVEL "BDD: Initialization completed successfully\n");
    return 0;

out_free_disk:
    put_disk(dev->gd);
out_free_queue:
    blk_mq_destroy_queue(dev->queue);
out_free_tags:
    blk_mq_free_tag_set(&dev->tag_set);
out_free_mem:
    vfree(dev->data);
out_free_dev:
    kfree(dev);
    return ret;
}

static void __exit myblock_exit(void)
{
    printk(DEBUG_LEVEL "BDD: Module unloading\n");
    
    if (dev) {
        if (atomic_read(&dev->initialized)) {
            del_gendisk(dev->gd);
            put_disk(dev->gd);
            blk_mq_destroy_queue(dev->queue);
            blk_mq_free_tag_set(&dev->tag_set);
        }
        vfree(dev->data);
        kfree(dev);
    }
    
    printk(DEBUG_LEVEL "BDD: Module unloaded\n");
}

module_init(myblock_init);
module_exit(myblock_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Stable Block Device Driver");
