#include <linux/module.h>
#include <linux/timer.h>

static struct timer_list timer;

static void timer_handler(struct timer_list *timer)
{
    pr_info("Timer at the address %p is active!\n", timer);
    mod_timer(timer, jiffies + 15 * HZ);
}

static int __init timer_module_init(void)
{
    timer.expires = jiffies + 15 * HZ;
    timer_setup(&timer, timer_handler, 0);
    add_timer(&timer);
    return 0;
}

static void __exit timer_module_exit(void)
{
    if (del_timer_sync(&timer))
        pr_notice("The timer was not active!\n");
}

module_init(timer_module_init);
module_exit(timer_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Muhammed Yavuz Berk Sener");
MODULE_DESCRIPTION("A module demonstrating the usage of low resolution timers.");
MODULE_VERSION("1.0");
