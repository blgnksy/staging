#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include "mysubsystem.h"

static struct task_struct *processor_task;

static int processor_thread(void *data)
{
    unsigned long count = 0;
    
    pr_info("Processor thread started (PID: %d)\n", current->pid);
    
    while (!kthread_should_stop()) {
        // Process data/events
        pr_info("Processor: handling task #%lu\n", count++);
        
        // Example: You might process a queue, handle events, etc.
        // if (has_pending_work()) {
        //     process_work();
        // }
        
        // Sleep for 2 seconds (different from worker and monitor)
        msleep(2000);
    }
    
    pr_info("Processor thread exiting after %lu tasks\n", count);
    return 0;
}

int processor_init(void)
{
    pr_info("Initializing processor thread\n");
    
    processor_task = kthread_run(processor_thread, NULL, "mysubsys_proc");
    
    if (IS_ERR(processor_task)) {
        pr_err("Failed to create processor thread\n");
        return PTR_ERR(processor_task);
    }
    
    return 0;
}

void processor_exit(void)
{
    if (processor_task) {
        pr_info("Stopping processor thread\n");
        kthread_stop(processor_task);
        processor_task = NULL;
    }
}