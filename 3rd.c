#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define NAME "pseudochar"

static struct cdev next_cdev;
static struct cdev prev_cdev;
static dev_t next_dev;
static dev_t prev_dev;
static struct class *next_class;
static struct class *prev_class;
static struct device *next_device;
static struct device *prev_device;
static uint64_t next_number;
static uint64_t prev_number;
static DEFINE_MUTEX(next_mutex);
static DEFINE_MUTEX(prev_mutex);

// Forward declarations
static int next_open(struct inode *inode, struct file *filp);
static int prev_open(struct inode *inode, struct file *filp);
static int next_release(struct inode *inode, struct file *filp);
static int prev_release(struct inode *inode, struct file *filp);
static ssize_t next_read(struct file *filp, char __user *buf, size_t len, loff_t *offset);
static ssize_t prev_read(struct file *filp, char __user *buf, size_t len, loff_t *offset);
static ssize_t next_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset);
static ssize_t prev_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset);

// File operations for nextdev
static struct file_operations next_fops = {
    .owner = THIS_MODULE,
    .open = next_open,
    .release = next_release,
    .read = next_read,
    .write = next_write,
};

// File operations for prevdev
static struct file_operations prev_fops = {
    .owner = THIS_MODULE,
    .open = prev_open,
    .release = prev_release,
    .read = prev_read,
    .write = prev_write,
};

// Implementation of open function for nextdev
static int next_open(struct inode *inode, struct file *filp)
{
    mutex_lock(&next_mutex);
    return 0;
}

// Implementation of open function for prevdev
static int prev_open(struct inode *inode, struct file *filp)
{
    mutex_lock(&prev_mutex);
    return 0;
}

// Implementation of release function for nextdev
static int next_release(struct inode *inode, struct file *filp)
{
    mutex_unlock(&next_mutex);
    return 0;
}

// Implementation of release function for prevdev
static int prev_release(struct inode *inode, struct file *filp)
{
    mutex_unlock(&prev_mutex);
    return 0;
}

// Implementation of read function for nextdev
static ssize_t next_read(struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
    uint64_t number = next_number + 1;
    char num[20];
    int ret;

    snprintf(num, sizeof(num), "%llu\n", number);

    ret = simple_read_from_buffer(buf, len, offset, num, strlen(num));
    if (ret > 0)
        next_number++;

    return ret;
}

// Implementation of read function for prevdev
// Implementation of read function for prevdev
static ssize_t prev_read(struct file *filp, char __user *buf, size_t len, loff_t *offset)
{
    if (prev_number > 0) {
        uint64_t number = prev_number - 1;
        char num[20];
        int ret;

        snprintf(num, sizeof(num), "%llu\n", number);

        ret = simple_read_from_buffer(buf, len, offset, num, strlen(num));
        if (ret > 0)
            prev_number--;

        return ret;
    }

    // If prev_number is already zero, return an end-of-file (EOF) condition
    return 0;
}


// Implementation of write function for nextdev
static ssize_t next_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset)
{
    char num[20];
    int ret;

    ret = simple_write_to_buffer(num, sizeof(num) - 1, offset, buf, len);
    if (ret > 0) {
        num[ret] = '\0';
        kstrtoull(num, 10, &next_number);
    }

    return ret;
}

// Implementation of write function for prevdev
static ssize_t prev_write(struct file *filp, const char __user *buf, size_t len, loff_t *offset)
{
    char num[20];
    int ret;

    ret = simple_write_to_buffer(num, sizeof(num) - 1, offset, buf, len);
    if (ret > 0) {
        num[ret] = '\0';
        kstrtoull(num, 10, &prev_number);
    }

    return ret;
}

static int __init pseudochar_init(void)
{
    int ret;

    // Allocate device numbers
    ret = alloc_chrdev_region(&next_dev, 0, 1, "nextdev");
    if (ret < 0) {
        printk(KERN_ALERT "[pseudochar]: Error allocating nextdev device number\n");
        return ret;
    }

    ret = alloc_chrdev_region(&prev_dev, 0, 1, "prevdev");
    if (ret < 0) {
        printk(KERN_ALERT "[pseudochar]: Error allocating prevdev device number\n");
        unregister_chrdev_region(next_dev, 1);
        return ret;
    }

    // Create class
    next_class = class_create(THIS_MODULE, "nextdev");
    if (IS_ERR(next_class)) {
        printk(KERN_ALERT "[pseudochar]: Error creating nextdev class\n");
        unregister_chrdev_region(next_dev, 1);
        unregister_chrdev_region(prev_dev, 1);
        return PTR_ERR(next_class);
    }

    prev_class = class_create(THIS_MODULE, "prevdev");
    if (IS_ERR(prev_class)) {
        printk(KERN_ALERT "[pseudochar]: Error creating prevdev class\n");
        class_destroy(next_class);
        unregister_chrdev_region(next_dev, 1);
        unregister_chrdev_region(prev_dev, 1);
        return PTR_ERR(prev_class);
    }

    // Initialize character devices
    cdev_init(&next_cdev, &next_fops);
    ret = cdev_add(&next_cdev, next_dev, 1);
    if (ret < 0) {
        printk(KERN_ALERT "[pseudochar]: Error adding nextdev cdev\n");
        class_destroy(next_class);
        unregister_chrdev_region(next_dev, 1);
        class_destroy(prev_class);
        unregister_chrdev_region(prev_dev, 1);
        return ret;
    }

    cdev_init(&prev_cdev, &prev_fops);
    ret = cdev_add(&prev_cdev, prev_dev, 1);
    if (ret < 0) {
        printk(KERN_ALERT "[pseudochar]: Error adding prevdev cdev\n");
        cdev_del(&next_cdev);
        class_destroy(next_class);
        unregister_chrdev_region(next_dev, 1);
        class_destroy(prev_class);
        unregister_chrdev_region(prev_dev, 1);
        return ret;
    }

    // Create devices
    next_device = device_create(next_class, NULL, next_dev, NULL, "nextdev");
    if (IS_ERR(next_device)) {
        printk(KERN_ALERT "[pseudochar]: Error creating nextdev device\n");
        cdev_del(&next_cdev);
        class_destroy(next_class);
        unregister_chrdev_region(next_dev, 1);
        cdev_del(&prev_cdev);
        class_destroy(prev_class);
        unregister_chrdev_region(prev_dev, 1);
        return PTR_ERR(next_device);
    }

    prev_device = device_create(prev_class, NULL, prev_dev, NULL, "prevdev");
    if (IS_ERR(prev_device)) {
        printk(KERN_ALERT "[pseudochar]: Error creating prevdev device\n");
        device_destroy(next_class, next_dev);
        cdev_del(&next_cdev);
        class_destroy(next_class);
        unregister_chrdev_region(next_dev, 1);
        cdev_del(&prev_cdev);
        class_destroy(prev_class);
        unregister_chrdev_region(prev_dev, 1);
        return PTR_ERR(prev_device);
    }

    return 0;
}

static void __exit pseudochar_exit(void)
{
    device_destroy(next_class, next_dev);
    cdev_del(&next_cdev);
    class_destroy(next_class);
    unregister_chrdev_region(next_dev, 1);

    device_destroy(prev_class, prev_dev);
    cdev_del(&prev_cdev);
    class_destroy(prev_class);
    unregister_chrdev_region(prev_dev, 1);
}

module_init(pseudochar_init);
module_exit(pseudochar_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Muhammed Yavuz Berk Şener");
MODULE_DESCRIPTION("Pseudo character device driver with nextdev and prevdev");
MODULE_VERSION("1.0");


