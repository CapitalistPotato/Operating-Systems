#include <linux/module.h>
#include <linux/workqueue.h>

static struct workqueue_struct *queue;

static void normal_work_handler(struct work_struct *work)
{
    pr_info("Hi! I'm handler of normal work!\n");
}

static void delayed_work_handler(struct work_struct *work)
{
    pr_info("Hi! I'm handler of delayed work!\n");
}

static DECLARE_WORK(normal_work, normal_work_handler);
static DECLARE_DELAYED_WORK(delayed_work, delayed_work_handler);

static int __init workqueue_module_init(void)
{
    queue = create_workqueue("works");
    if (!queue)
    {
        pr_alert("[workqueue_module] Error creating a workqueue\n");
        return -ENOMEM;
    }

    if (schedule_work(&normal_work))
        pr_info("The normal work was already queued!\n");
    if (schedule_delayed_work(&delayed_work, 10 * HZ))
        pr_info("The delayed work was already queued!\n");

    return 0;
}

static void __exit workqueue_module_exit(void)
{
    if (cancel_work_sync(&normal_work))
        pr_info("The normal work has not been done yet!\n");
    if (cancel_delayed_work_sync(&delayed_work))
        pr_info("The delayed work has not been done yet!\n");

    flush_scheduled_work();
    destroy_workqueue(queue);
}

module_init(workqueue_module_init);
module_exit(workqueue_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Muhammed Yavuz Berk Sener");
MODULE_DESCRIPTION("A module demonstrating the use of work queues.");
MODULE_VERSION("1.0");
