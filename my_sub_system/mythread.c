#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/delay.h>

static struct task_struct *my_kthread;

static int my_thread_function(void *data)
{
    int count =0;

    pr_info("Kernel thread started");

    while (!kthread_should_stop()) {
        pr_info("Kernel thread running: iteration = %d", count++);
        // Thread work goes here
        ssleep(5); // Sleep for 5 seconds


    }

    pr_info("Kernel thread stopping");
    return 0;
}

static int __init my_thread_init(void)
{
    pr_info("Creating kernel thread\n");
    my_kthread = kthread_run(my_thread_function, NULL, "my_kthread");
    if (IS_ERR(my_kthread)) {
        pr_err("Failed to create kernel thread\n");
        return PTR_ERR(my_kthread);
    }
    wake_up_process(my_kthread);
    return 0;
}