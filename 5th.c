#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "clipboard"
#define MAX_BUFFER_SIZE 1024

static struct cdev clipboard_cdev;
static dev_t clipboard_dev;
static struct class *clipboard_class;
static struct device *clipboard_device;
static char clipboard_buffer[MAX_BUFFER_SIZE];
static DEFINE_MUTEX(clipboard_mutex);

static int clipboard_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int clipboard_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t clipboard_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t bytes_read = 0;

    mutex_lock(&clipboard_mutex);

    if (*f_pos >= MAX_BUFFER_SIZE) {
        mutex_unlock(&clipboard_mutex);
        return bytes_read; // End of file
    }

    if (*f_pos + count > MAX_BUFFER_SIZE)
        count = MAX_BUFFER_SIZE - *f_pos;

    if (copy_to_user(buf, &clipboard_buffer[*f_pos], count)) {
        mutex_unlock(&clipboard_mutex);
        return -EFAULT;
    }

    *f_pos += count;
    bytes_read = count;

    mutex_unlock(&clipboard_mutex);

    return bytes_read;
}

static ssize_t clipboard_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t bytes_written = 0;

    mutex_lock(&clipboard_mutex);

    if (*f_pos >= MAX_BUFFER_SIZE) {
        mutex_unlock(&clipboard_mutex);
        return bytes_written; // End of file
    }

    if (*f_pos + count > MAX_BUFFER_SIZE)
        count = MAX_BUFFER_SIZE - *f_pos;

    if (copy_from_user(&clipboard_buffer[*f_pos], buf, count)) {
        mutex_unlock(&clipboard_mutex);
        return -EFAULT;
    }

    *f_pos += count;
    bytes_written = count;

    mutex_unlock(&clipboard_mutex);

    return bytes_written;
}

static struct file_operations clipboard_fops = {
    .owner = THIS_MODULE,
    .open = clipboard_open,
    .release = clipboard_release,
    .read = clipboard_read,
    .write = clipboard_write,
};

static int __init clipboard_init(void)
{
    if (alloc_chrdev_region(&clipboard_dev, 0, 1, DEVICE_NAME) < 0) {
        printk(KERN_ALERT "Failed to allocate device numbers\n");
        return -1;
    }

    cdev_init(&clipboard_cdev, &clipboard_fops);

    if (cdev_add(&clipboard_cdev, clipboard_dev, 1) < 0) {
        printk(KERN_ALERT "Failed to add character device\n");
        unregister_chrdev_region(clipboard_dev, 1);
        return -1;
    }

    clipboard_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (IS_ERR(clipboard_class)) {
        printk(KERN_ALERT "Failed to create device class\n");
        cdev_del(&clipboard_cdev);
        unregister_chrdev_region(clipboard_dev, 1);
        return PTR_ERR(clipboard_class);
    }

    clipboard_device = device_create(clipboard_class, NULL, clipboard_dev, NULL, DEVICE_NAME);
    if (IS_ERR(clipboard_device)) {
        printk(KERN_ALERT "Failed to create device\n");
        class_destroy(clipboard_class);
        cdev_del(&clipboard_cdev);
        unregister_chrdev_region(clipboard_dev, 1);
        return PTR_ERR(clipboard_device);
    }

    mutex_init(&clipboard_mutex);

    printk(KERN_INFO "Clipboard device initialized\n");

    return 0;
}

static void __exit clipboard_exit(void)
{
    mutex_destroy(&clipboard_mutex);
    device_destroy(clipboard_class, clipboard_dev);
    class_destroy(clipboard_class);
    cdev_del(&clipboard_cdev);
    unregister_chrdev_region(clipboard_dev, 1);

    printk(KERN_INFO "Clipboard device unloaded\n");
}

module_init(clipboard_init);
module_exit(clipboard_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yavuzberk Şener");
MODULE_DESCRIPTION("Pseudo character device for clipboard");
MODULE_VERSION("1.0");
