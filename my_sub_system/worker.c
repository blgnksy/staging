#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "mysubsystem.h"

static struct task_struct *worker_task;

static int worker_thread(void *data)
{
    while (!kthread_should_stop()) {
        // Perform periodic work here
        pr_info("Worker: Worker thread is running\n");
        
        // Simulate work by sleeping
        msleep(1000);
    }
    pr_info("Worker: Worker thread stopping\n");
    return 0;
}

int worker_init(void){
    worker_task = kthread_run(worker_thread, NULL, "my_worker_thread");
    if (IS_ERR(worker_task)) {
        pr_err("Worker: Failed to create worker thread\n");
        return PTR_ERR(worker_task);
    }
    pr_info("Worker: Worker thread started\n");
    return 0;
}

void worker_exit(void){
    if (worker_task) {
        kthread_stop(worker_task);
        pr_info("Worker: Worker thread stopped\n");
    }
}


