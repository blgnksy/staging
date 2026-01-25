#ifndef SYNCHRONIZATION_COMMON_IOCTL_H
#define SYNCHRONIZATION_COMMON_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#include <stdint.h> /* for int32_t used below */
#endif

/* Device name (userspace convenience) */
#define SEMAPHORES_DEV_PATH "/dev/semaphores"
#define SEMAPHORES_DEVICE_NAME "semaphores"  /* Dev name as it appears in /proc/devices   */
#define MUTEXES_DEV_PATH "/dev/mutexes"
#define MUTEXES_DEVICE_NAME "mutexes"  /* Dev name as it appears in /proc/devices   */

/* IOCTLs shared between kernel and userspace */
#define SEMAPHORES_IOC_MAGIC 'k'
#define SEMAPHORES_IOC_SET_COUNTER _IOW(SEMAPHORES_IOC_MAGIC, 1, int32_t)
#define SEMAPHORES_IOC_GET_COUNTER _IOR(SEMAPHORES_IOC_MAGIC, 2, int32_t)
#define SEMAPHORES_IOC_RESET       _IO(SEMAPHORES_IOC_MAGIC, 3)

#define MUTEXES_IOC_MAGIC 'l'
#define MUTEXES_IOC_SET_COUNTER _IOW(MUTEXES_IOC_MAGIC, 1, int32_t)
#define MUTEXES_IOC_GET_COUNTER _IOR(MUTEXES_IOC_MAGIC, 2, int32_t)
#define MUTEXES_IOC_RESET       _IO(MUTEXES_IOC_MAGIC, 3)
#endif /* SYNCHRONIZATION_COMMON_IOCTL_H */