#include <linux/module.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/blkdev.h>

#define DEVICE_SIZE 4 * 1024 * 1024

static int sector_size = 512;
static int major = 0;
static struct sbd_struct {
    struct gendisk *gd;
    struct gendisk *gd1; // New partition 1
    struct gendisk *gd2; // New partition 2
    void *memory;
} sbd_dev;

static inline int transfer_single_bio(struct bio *bio) {
    struct bvec_iter iter;
    struct bio_vec vector;
    sector_t sector = bio->bi_iter.bi_sector;
    bool write = bio_data_dir(bio) == WRITE;

    bio_for_each_segment(vector, bio, iter) {
        unsigned int len = vector.bv_len;
        void *addr = kmap(vector.bv_page);
        if (write)
            memcpy(sbd_dev.memory + sector * sector_size, addr + vector.bv_offset, len);
        else
            memcpy(addr + vector.bv_offset, sbd_dev.memory + sector * sector_size, len);
        kunmap(addr);
        sector += len >> 9;
    }
    return 0;
}

static blk_qc_t make_request(struct request_queue *q, struct bio *bio) {
    int result = 0;

    if (bio_end_sector(bio) > get_capacity(bio->bi_bdev->bd_disk))
        goto mrerr0;

    result = transfer_single_bio(bio);
    if (unlikely(result != 0))
        goto mrerr0;

    bio_endio(bio);
    return BLK_QC_T_NONE;
mrerr0:
    bio_io_error(bio);
    return BLK_QC_T_NONE;
}

static struct block_device_operations block_methods = {
    .owner = THIS_MODULE,
};

static int __init sbd_constructor(void) {
    sbd_dev.memory = vmalloc(DEVICE_SIZE);
    if (!sbd_dev.memory) {
        pr_alert("Memory allocation error!\n");
        goto ier1;
    }
    
    // Allocate main gendisk
    sbd_dev.gd = alloc_disk(1);
    if (!sbd_dev.gd) {
        pr_alert("General disk structure allocation error!\n");
        goto ier2;
    }
    
    // Allocate partition 1 gendisk
    sbd_dev.gd1 = alloc_disk(1);
    if (!sbd_dev.gd1) {
        pr_alert("Partition 1 disk structure allocation error!\n");
        goto ier3;
    }
    
    // Allocate partition 2 gendisk
    sbd_dev.gd2 = alloc_disk(1);
    if (!sbd_dev.gd2) {
        pr_alert("Partition 2 disk structure allocation error!\n");
        goto ier4;
    }
    
    major = register_blkdev(major, "sbd");
    if (major <= 0) {
        pr_alert("Major number allocation error!\n");
        goto ier5;
    }
    pr_info("[sbd] Major number allocated: %d.\n", major);
    
    // Main gendisk initialization
    sbd_dev.gd->major = major;
    sbd_dev.gd->first_minor = 0;
    sbd_dev.gd->fops = &block_methods;
    sbd_dev.gd->private_data = NULL;
    sbd_dev.gd->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
    strcpy(sbd_dev.gd->disk_name, "sbd");
    set_capacity(sbd_dev.gd, (DEVICE_SIZE) >> 9);
    sbd_dev.gd->queue = blk_alloc_queue(GFP_KERNEL);
    if (!sbd_dev.gd->queue) {
        pr_alert("Request queue allocation error!\n");
        goto ier6;
    }
    blk_queue_make_request(sbd_dev.gd->queue, make_request);
    pr_info("[sbd] Gendisk initialized.\n");

    // Partition 1 initialization
    sbd_dev.gd1->major = major;
    sbd_dev.gd1->first_minor = 1;
    sbd_dev.gd1->fops = &block_methods;
    sbd_dev.gd1->private_data = NULL;
    sbd_dev.gd1->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
    strcpy(sbd_dev.gd1->disk_name, "sbd1");
    set_capacity(sbd_dev.gd1, (DEVICE_SIZE) >> 9);
    sbd_dev.gd1->queue = sbd_dev.gd->queue; // Use the same queue as the main gendisk

    // Partition 2 initialization
    sbd_dev.gd2->major = major;
    sbd_dev.gd2->first_minor = 2;
    sbd_dev.gd2->fops = &block_methods;
    sbd_dev.gd2->private_data = NULL;
    sbd_dev.gd2->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
    strcpy(sbd_dev.gd2->disk_name, "sbd2");
    set_capacity(sbd_dev.gd2, (DEVICE_SIZE) >> 9);
    sbd_dev.gd2->queue = sbd_dev.gd->queue; // Use the same queue as the main gendisk

    add_disk(sbd_dev.gd);
    add_disk(sbd_dev.gd1); // Add partition 1 gendisk
    add_disk(sbd_dev.gd2); // Add partition 2 gendisk
    return 0;

ier6:
    unregister_blkdev(major, "sbd");
ier5:
    put_disk(sbd_dev.gd2);
ier4:
    put_disk(sbd_dev.gd1);
ier3:
    put_disk(sbd_dev.gd);
ier2:
    vfree(sbd_dev.memory);
ier1:
    return -ENOMEM;
}

static void __exit sbd_desctructor(void) {
    del_gendisk(sbd_dev.gd2);
    del_gendisk(sbd_dev.gd1);
    del_gendisk(sbd_dev.gd);
    blk_cleanup_queue(sbd_dev.gd->queue);
    unregister_blkdev(major, "sbd");
    put_disk(sbd_dev.gd2);
    put_disk(sbd_dev.gd1);
    put_disk(sbd_dev.gd);
    vfree(sbd_dev.memory);
}

module_init(sbd_constructor);
module_exit(sbd_desctructor);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Muhammed Yavuz Berk Sener");
MODULE_DESCRIPTION("A pseudo block device.");
MODULE_VERSION("1.0");