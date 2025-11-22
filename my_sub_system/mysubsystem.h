#ifndef _MYSUBSYSTEM_H
#define _MYSUBSYSTEM_H

#include <linux/kthread.h>

// Worker Thread
int worker_init(void);
void worker_exit(void);

// Monitor Thread
int monitor_init(void);
void monitor_exit(void);

// Processor Thread
int processor_init(void);
void processor_exit(void);

#endif // _MYSUBSYSTEM_H