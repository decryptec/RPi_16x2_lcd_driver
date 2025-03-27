#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>

#include "lcd_driver.h"

const int rs = 0, en = 5, d4 = 6, d5 = 13, d6 = 19, d7 = 26;

struct lcd_pins u_pins = {rs, en, d4, d5, d6, d7}; 

int main() {
    int cursor_pos = 0;
    int test_left = 20;
    int test_right = 20;
    int dev = open("/dev/lcd0", O_RDWR); // Open the device file for read and write
    if (dev == -1) {
        perror("Failed to open LCD device");
        return -1;
    }

    // Initialize the LCD with user-defined pins
    if (ioctl(dev, INIT, &u_pins) == -1) {
        perror("Failed to initialize LCD");
        close(dev);
        return -1;
    }
    sleep(2);

    // Clear the LCD display before any writing
    if (ioctl(dev, CLEAR) == -1) {
        perror("Failed to clear LCD display");
        close(dev);
        return -1;
    }

    // Set cursor position on Line 1
    if (ioctl(dev, LINE_1, &cursor_pos) == -1) {
        perror("Failed to set cursor position on LINE_1");
        close(dev);
        return -1;
    }
    //write(dev, "Line 1: Testing...", strlen("Line 1: Testing..."));
	write(dev, "Line 1: Testing...0 1 2 3 4 5 6 7 8 9 10", strlen("Line 2: Testing...0 1 2 3 4 5 6 7 8 9 10")); // 41 char > MAX_BUFFER_SIZE, truncates

    printf("Written to Line 1: 'Line 1: Testing...'\n");
    sleep(1);  // Wait for a moment to see the output

    // Set cursor position on Line 2
    if (ioctl(dev, LINE_2, &cursor_pos) == -1) {
        perror("Failed to set cursor position on LINE_2");
        close(dev);
        return -1;
    }
    write(dev, "Line 2: Testing...", strlen("Line 2: Testing..."));
    printf("Written to Line 2: 'Line 2: Testing...'\n");
    sleep(1);  // Wait for a moment to see the output

    // Test scrolling to the left
    if (ioctl(dev, SCROLL_LEFT, &test_left) == -1) {
        perror("Failed to enable scrolling left");
        close(dev);
        return -1;
    }
    printf("Scrolling left with %d steps...\n", test_left);
    sleep(1);

    // Test scrolling to the right
    if (ioctl(dev, SCROLL_RIGHT, &test_right) == -1) {
        perror("Failed to enable scrolling right");
        close(dev);
        return -1;
    }
    printf("Scrolling right with %d steps...\n", test_right);
    sleep(1);

    // Overwrite part of Line 1 at column 2
    cursor_pos = 2;
    if (ioctl(dev, LINE_1, &cursor_pos) == -1) {
        perror("Failed to set cursor position on LINE_1");
        close(dev);
        return -1;
    }
    write(dev, "(Overwrite at col 2)", strlen("(Overwrite at col 2)"));
    printf("Written to Line 1: '(Overwrite at col 2)'\n");
    sleep(1);  // Wait for a moment to see the output

    // Attempt to read the content written to the LCD (if supported by the driver)
    char read_buffer[MAX_BUFFER_SIZE] = {0};  // Ensure buffer is initialized
    lseek(dev, 0, SEEK_SET); // Rewind to the start for reading
    ssize_t bytes_read = read(dev, read_buffer, sizeof(read_buffer) - 1);
    if (bytes_read == -1) {
        perror("Failed to read from LCD device (read may not be implemented)");
        close(dev);
        return -1;
    }

    // Null-terminate and print the buffer read from the LCD
    read_buffer[bytes_read] = '\0';
    printf("Content read from LCD device: %s\n", read_buffer);

	// Test scrolling to the left
	test_left = 3;
	if (ioctl(dev, SCROLL_LEFT, &test_left) == -1) {
		perror("Failed to enable scrolling left");
		close(dev);
		return -1;
	}
	printf("Scrolling left with %d steps...\n", test_left);
	sleep(1);

    // Clean up and close the device
    close(dev);
    printf("LCD device closed\n");

    return 0;
}
