// ...existing code...
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "semaphores.h"

int main(void)
{
    int fd = open("/dev/semaphores", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    int val = 42;
    if (ioctl(fd, SEMAPHORES_IOC_SET_COUNTER, &val) < 0) perror("SET");

    val = 0;
    if (ioctl(fd, SEMAPHORES_IOC_GET_COUNTER, &val) < 0) perror("GET");
    printf("counter = %d\n", val);

    if (ioctl(fd, SEMAPHORES_IOC_RESET) < 0) perror("RESET");

    close(fd);
    return 0;
}