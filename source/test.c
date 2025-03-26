#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <string.h>

// ioctl cmds
#define INIT _IOW('a', 'a', int32_t *)
#define CLEAR _IO('a', 'b')
#define LINE_1 _IO('a', 'c')
#define LINE_2 _IO('a', 'd')
#define scroll_left _IOW('a', 'e', int32_t *)
#define scroll_right _IOW('a', 'f', int32_t *)


int main() {
	int test_left = 5;
	int test_right = 8;
    int dev = open("/dev/lcd0", O_WRONLY);
    if (dev == -1) {
        perror("Failed to open LCD device");
        return -1;
    }

    if (ioctl(dev, INIT, 1) == -1) {
        perror("Failed to initialize LCD");
    }

    if (ioctl(dev, CLEAR) == -1) {
        perror("Failed to clear LCD display");
    }

    if (ioctl(dev, LINE_1) == -1) {
        perror("Failed to disable cursor");
    }
	write(dev, "Line 1 testing", strlen("Line 1 testing"));
	printf("\nLength of string: %d \n", strlen("Line 1 testing")); 
	sleep(1);

    if (ioctl(dev, LINE_2) == -1) {
        perror("Failed to set cursor position");
    }
	write(dev, "Line 2 testing", strlen("Line 2 testing"));
	sleep(1);
    if (ioctl(dev, scroll_left, &test_left) == -1) {
        perror("Failed to enable scrolling");
    }
	sleep(1);
    if (ioctl(dev, scroll_right, &test_right) == -1) {
        perror("Failed to enable scrolling");
    }
    	sleep(1);
    close(dev);
    return 0;
}

