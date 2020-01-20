#include <stdio.h>
#include <stdlib.h>
#include <linux/rtc.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/ioctl.h>

#define ERROR -1

int main()
{

    int fd = open("/dev/rtc", O_RDWR | O_NONBLOCK);

    if (fd == ERROR) {
        perror("open");
        return -1;
    }

    int epoch = 5;

    if (ioctl(fd, RTC_EPOCH_SET, epoch) == ERROR) {
        perror("ioctl");
        return -1;
    }

    printf("Epoch set!\n", epoch);

    return 0;
}
