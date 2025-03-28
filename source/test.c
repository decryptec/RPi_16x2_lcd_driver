#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include "lcd_driver.h"

// Define LCD pin connections
const int rs = 0, en = 5, d4 = 6, d5 = 13, d6 = 19, d7 = 26;
struct lcd_pins u_pins = {rs, en, d4, d5, d6, d7};

char usr_buf[MAX_BUFFER_SIZE];

int main() {
    int cursor_pos = 0;
    const char *line1 = "Line 1: Truncate overflow test.........";
    const char *overwrite = "(Overwriting at cursor)";
    int test_left = 20;
    int test_right = 20;
    int num = 42;
    float pi = 3.14;

    // Open LCD device
    int dev = open("/dev/lcd0", O_RDWR);
    if (dev == -1) {
        perror("Failed to open LCD device");
        return -1;
    }

    // Initialize LCD with defined pins
    if (ioctl(dev, INIT, &u_pins) == -1) {
        perror("Failed to initialize LCD");
        close(dev);
        return -1;
    }
    printf("Initialize LCD\n");
    sleep(2);

    // Clear LCD display
    if (ioctl(dev, CLEAR) == -1) {
        perror("Failed to clear LCD display");
        close(dev);
        return -1;
    }

    // Write to Line 1
    if (ioctl(dev, LINE_1, &cursor_pos) == -1) {
        perror("Failed to set cursor position on LINE_1");
        close(dev);
        return -1;
    }
    write(dev, line1, strlen(line1));
    printf("Written to Line 1: '%s'\n", line1);
    sleep(1);

    // Write to Line 2
    if (ioctl(dev, LINE_2, &cursor_pos) == -1) {
        perror("Failed to set cursor position on LINE_2");
        close(dev);
        return -1;
    }
    write(dev, "Line 2: Testing...", strlen("Line 2: Testing..."));
    printf("Written to Line 2: 'Line 2: Testing...'\n");
    sleep(1);

    // Scroll left
    printf("Scrolling left with %d steps...\n", test_left);
    if (ioctl(dev, SCROLL_LEFT, &test_left) == -1) {
        perror("Failed to enable scrolling left");
        close(dev);
        return -1;
    }
    sleep(1);

    // Scroll right
    printf("Scrolling right with %d steps...\n", test_right);
    if (ioctl(dev, SCROLL_RIGHT, &test_right) == -1) {
        perror("Failed to enable scrolling right");
        close(dev);
        return -1;
    }
    sleep(1);

    // Overwrite text at a specific cursor position on Line 1
    cursor_pos = 2;
    if (ioctl(dev, LINE_1, &cursor_pos) == -1) {
        perror("Failed to set cursor position on LINE_1");
        close(dev);
        return -1;
    }
    write(dev, overwrite, strlen(overwrite));
    printf("Written to Line 1: '%s'\n", overwrite);
    
    // Scroll left with a different step count
    test_left = 10;
    printf("Scrolling left with %d steps...\n", test_left);
    if (ioctl(dev, SCROLL_LEFT, &test_left) == -1) {
        perror("Failed to enable scrolling left");
        close(dev);
        return -1;
    }
    sleep(1);

    // Read content from char device. Last write content
    char read_buffer[MAX_BUFFER_SIZE] = {0};
    lseek(dev, 0, SEEK_SET);
    ssize_t bytes_read = read(dev, read_buffer, sizeof(read_buffer) - 1);
    if (bytes_read == -1) {
        perror("Failed to read from LCD device");
        close(dev);
        return -1;
    }
    read_buffer[bytes_read] = '\0';
    printf("Content read from LCD device: %s\n", read_buffer);

    sleep(1);

    // Display integer and float values on LCD
    ioctl(dev,CLEAR);
    snprintf(usr_buf, sizeof(usr_buf), "Int: %d", num);
    write(dev, usr_buf, strlen(usr_buf));

    snprintf(usr_buf, sizeof(usr_buf), "Float: %.2f", pi);
    cursor_pos = 0;
    ioctl(dev, LINE_2, &cursor_pos);
    write(dev, usr_buf, strlen(usr_buf));
    sleep(3);

    // Final message display
    ioctl(dev, CLEAR);
    cursor_pos = 0;
    ioctl(dev, LINE_1, &cursor_pos);
    write(dev, "Test complete", strlen("Test complete"));

    // Close LCD device
    close(dev);
    printf("LCD device closed\n");

    return 0;
}

