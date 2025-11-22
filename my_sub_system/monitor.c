#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/sched/signal.h>
#include "mysubsystem.h"

static struct task_struct *monitor_task;

/* Monitor file descriptors of the current kernel thread */
static void monitor_file_descriptors(void)
{
    /*
    ┌──────────────┐      ┌──────────────┐      ┌──────────────┐      ┌──────────────┐
    │              │      │              │      │              │      │              │
    │ task_struct  │─────▶│ files_struct │─────▶│   fdtable    │─────▶│ fd[] array   │
    │              │files │              │ fdt  │              │ fd   │              │
    └──────────────┘      └──────────────┘      └──────────────┘      └──────────────┘
                                                                            │
                                                                            ▼
                                                                        ┌──────────────┐
                                                                        │              │
                                                                        │ struct file  │
                                                                        │              │
                                                                        └──────────────┘
    
    ┌──────────────┐        ┌──────────────┐     
    │              │        │              │     
    │ struct file  │─────▶  │ inode        │     
    │              │f_inode │              │     
    └──────────────┘        └──────────────┘     
    
    ┌──────────────┐        ┌──────────────┐        ┌──────────────┐        ┌──────────────┐     
    │              │        │              │        │              │        │              │     
    │ struct file  │─────▶  │ path         │─────▶  │ dentry       │─────▶  │ inode        │     
    │              │f_path  │              │dentry  │              │d_inode │              │     
    └──────────────┘        └──────────────┘        └──────────────┘        └──────────────┘    
    */
    struct files_struct *files;
    struct fdtable *fdt;
    struct file *file;
    unsigned int fd;
    int open_count = 0;

    files = current->files;
    if (!files) {
        pr_info("Monitor: No files_struct for monitor thread (PID:%d)\n", current->pid);
        return;
    }

    spin_lock(&files->file_lock);
    fdt = files_fdtable(files);

    /* Count and display only OPEN file descriptors */
    for (fd = 0; fd < fdt->max_fds; fd++) {
        file = rcu_dereference_check(fdt->fd[fd],
                                     lockdep_is_held(&files->file_lock));
        if (file) {
            const struct path *path = &file->f_path;
            pr_info("Monitor: file->f_inode->i_ino=%lu "
			"and d_inode(path->dentry)->i_ino=%lu)\n",
			file->f_inode->i_ino,
			d_inode(path->dentry)->i_ino);
            char *buf = (char *)__get_free_page(GFP_ATOMIC);

            if (buf) {
                char *pathname = d_path(path, buf, PAGE_SIZE);
                if (!IS_ERR(pathname)) {
                    pr_info("Monitor:   fd[%d] -> %s (flags: 0x%x, pos: %lld)\n",
                            fd, pathname, file->f_flags, file->f_pos);
                } else {
                    pr_info("Monitor:   fd[%d] -> (path error: %ld)\n",
                            fd, PTR_ERR(pathname));
                }
                free_page((unsigned long)buf);
            } else {
                pr_info("Monitor:   fd[%d] -> (no memory for path)\n", fd);
            }
            open_count++;
        }
    }

    if (open_count == 0) {
        pr_info("Monitor: No open file descriptors (max_fds: %d)\n", fdt->max_fds);
    } else {
        pr_info("Monitor: Total open fds: %d/%d\n", open_count, fdt->max_fds);
    }

    spin_unlock(&files->file_lock);
}



/* Create a test file to have something to monitor */
static int test_fd = -1;

static void create_test_file(void)
{
    struct file *filp;
    int fd;

    pr_info("Monitor: create_test_file: current=%p, current->fs=%p\n", current, current->fs);

    filp = filp_open("/root/monitor_test.txt", O_CREAT | O_WRONLY, 0644);
    if (IS_ERR(filp)) {
        pr_err("Monitor: Failed to create test file (error %ld)\n", PTR_ERR(filp));
        test_fd = -1;
        return;
    }

    /* Get an unused file descriptor number */
    fd = get_unused_fd_flags(0);
    if (fd < 0) {
        pr_err("Monitor: Failed to allocate file descriptor\n");
        filp_close(filp, NULL);
        test_fd = -1;
        return;
    }

    /* Install the file in the file descriptor table */
    fd_install(fd, filp);
    test_fd = fd;
    pr_info("Monitor: Created test file /tmp/monitor_test.txt (fd=%d in current->files)\n", fd);
}

static void close_test_file(void)
{
    if (test_fd >= 0) {
        close_fd(test_fd);
        pr_info("Monitor: Closed test file (fd=%d)\n", test_fd);
        test_fd = -1;
    }
}

static int monitor_thread(void *data)
{
    unsigned long iteration = 0;

    pr_info("Monitor: Thread started (PID:%d, TID:%d)\n",
            current->tgid, current->pid);

    /* Create a test file to monitor */
    create_test_file();

    while (!kthread_should_stop()) {
        pr_info("Monitor: ===== Iteration %lu =====\n", iteration++);

        /* Monitor our own file descriptors */
        pr_info("Monitor: Checking own file descriptors...\n");
        monitor_file_descriptors();

        msleep(2000); /* Sleep for 2 seconds instead of 500ms to reduce spam */
    }

    /* Cleanup */
    close_test_file();

    pr_info("Monitor: Thread exiting\n");
    return 0;
}

int monitor_init(void)
{
    pr_info("Monitor: Initializing monitor thread\n");

    monitor_task = kthread_run(monitor_thread, NULL, "mysubsys_monitor");
    if (IS_ERR(monitor_task)) {
        pr_err("Monitor: Failed to create monitor thread\n");
        return PTR_ERR(monitor_task);
    }

    pr_info("Monitor: Thread created successfully\n");
    return 0;
}

void monitor_exit(void)
{
    if (monitor_task) {
        pr_info("Monitor: Stopping monitor thread\n");
        kthread_stop(monitor_task);
        monitor_task = NULL;
    }
}