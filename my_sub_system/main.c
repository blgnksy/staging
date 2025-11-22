#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include "mysubsystem.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bilgin Aksoy");
MODULE_DESCRIPTION("My Sub System with Worker, Monitor, and Processor Threads");

static int __init mysubsystem_init(void)
{
    int ret;

    ret = worker_init();
    if (ret) {
        pr_err("Failed to initialize worker thread\n");
        return ret;
    }

    ret = monitor_init();
    if (ret) {
        pr_err("Failed to initialize monitor thread\n");
        worker_exit();
        return ret;
    }

    ret = processor_init();
    if (ret) {
        pr_err("Failed to initialize processor thread\n");
        monitor_exit();
        worker_exit();
        return ret;
    }

    pr_info("My Sub System initialized successfully\n");
    return 0;
}

static void __exit mysubsystem_exit(void)
{
    processor_exit();
    monitor_exit();
    worker_exit();
    pr_info("My Sub System exited successfully\n");
}

module_init(mysubsystem_init);
module_exit(mysubsystem_exit);

