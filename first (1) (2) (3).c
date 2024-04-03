#include <linux/module.h>

static int __init first_init(void)
{
    printk(KERN_ALERT "%d %s\n", __LINE__, __FILE__);
    return 0;
}

static void __exit first_exit(void)
{
    printk(KERN_ALERT "%d %s\n", __LINE__, __FILE__);
}

module_init(first_init);
module_exit(first_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Muhammed Yavuz Berk Sener");
MODULE_DESCRIPTION("Another \"Hello World!\" kernel module :-)");
MODULE_VERSION("1.0");
