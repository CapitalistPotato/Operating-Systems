#include <linux/module.h>
#include <linux/genhd.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/blkdev.h>

#define DEVICE_SIZE (4 * 1024 * 1024) 

static int sector_size = 512;
static int major = 0;

static struct sbd_struct {
    struct gendisk *gd;
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
            memcpy((void *)(sbd_dev.memory + sector * sector_size), (void *)(addr + vector.bv_offset), len);
        else
            memcpy((void *)(addr + vector.bv_offset), (void *)(sbd_dev.memory + sector * sector_size), len);
        kunmap(addr);
        sector += len >> 9;
    }
    return 0;
}

static blk_qc_t make_request(struct request_queue *q, struct bio *bio) {
    int result = 0;

    if (bio_end_sector(bio) > get_capacity(bio->bi_bdev->bd_disk)) {
        bio_io_error(bio);
        return BLK_QC_T_NONE;
    }

    result = transfer_single_bio(bio);
    if (unlikely(result != 0)) {
        bio_io_error(bio);
        return BLK_QC_T_NONE;
    }

    bio_endio(bio);
    return BLK_QC_T_NONE;
}

static struct block_device_operations block_methods = {
    .owner = THIS_MODULE
};

static int __init sbd_constructor(void) {
    sbd_dev.memory = vmalloc(DEVICE_SIZE);
    if (!sbd_dev.memory) {
        pr_alert("Memory allocation error!\n");
        return -ENOMEM;
    }

    sbd_dev.gd = alloc_disk(1);
    if (!sbd_dev.gd) {
        pr_alert("General disk structure allocation error!\n");
        vfree(sbd_dev.memory);
        return -ENOMEM;
    }

    major = register_blkdev(major, "sbd");
    if (major <= 0) {
        pr_alert("Major number allocation error!\n");
        put_disk(sbd_dev.gd);
        vfree(sbd_dev.memory);
        return -ENOMEM;
    }

    pr_info("[sbd] Major number allocated: %d.\n", major);
    sbd_dev.gd->major = major;
    sbd_dev.gd->first_minor = 0;
    sbd_dev.gd->fops = &block_methods;
    sbd_dev.gd->private_data = NULL;
    sbd_dev.gd->flags |= GENHD_FL_SUPPRESS_PARTITION_INFO;
    strcpy(sbd_dev.gd->disk_name, "sbd");
    set_capacity(sbd_dev.gd, DEVICE_SIZE >> 9);
    sbd_dev.gd->queue = blk_alloc_queue(GFP_KERNEL);
    if (!sbd_dev.gd->queue) {
        pr_alert("Request queue allocation error!\n");
        unregister_blkdev(major, "sbd");
        put_disk(sbd_dev.gd);
        vfree(sbd_dev.memory);
        return -ENOMEM;
    }

    blk_queue_make_request(sbd_dev.gd->queue, make_request);
    pr_info("[sbd] Gendisk initialized.\n");
    add_disk(sbd_dev.gd);
    return 0;
}

static void __exit sbd_desctructor(void) {
    del_gendisk(sbd_dev.gd);
    blk_cleanup_queue(sbd_dev.gd->queue);
    unregister_blkdev(major, "sbd");
    put_disk(sbd_dev.gd);
    vfree(sbd_dev.memory);
}

module_init(sbd_constructor);
module_exit(sbd_desctructor);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Muhammed Yavuz Berk Sener");
MODULE_DESCRIPTION("A pseudo block device.");
MODULE_VERSION("1.0");
