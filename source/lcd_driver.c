#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include "lcd_driver.h"


static struct gpio_desc *rs, *en, *data_pins[4];
static char buffer[MAX_BUFFER_SIZE];
static ssize_t buffer_size = 0;
static int major = 0;

static void lcd_byte(uint8_t bits, uint8_t mode);
static int write_full(char string[]);

static ssize_t my_read(struct file *filp, char __user *user_buffer, size_t count, loff_t *offset) {
    size_t len = buffer_size;

    // Check if offset exceeds buffer size
    if (*offset >= len)
        return 0;  // No data left to read

    // Limit read size to remaining data or user buffer size
    len = (count < len - *offset) ? count : len - *offset;

    // Copy data from kernel buffer to user buffer
    if (copy_to_user(user_buffer, buffer + *offset, len))
        return -EFAULT;

    // Update offset
    *offset += len;

    // Update LCD if buffer has data
    if (buffer_size > 0) {
        lcd_byte(LCD_CLEAR_DISPLAY, 0);
        lcd_byte(LCD_LINE_1, 0);
        msleep(5);
        write_full(buffer);
    }

    pr_info("lcd_driver - Read. Last written content: %s\n", buffer);
    return len;
}

static ssize_t my_write(struct file *filp, const char __user *user_buffer, size_t count, loff_t *offset) {
    // Limit count to avoid overflow
    if (count > MAX_BUFFER_SIZE)
    {
        pr_err("lcd_driver - Greater than MAX_BUFFER. Truncating");
        count = MAX_BUFFER_SIZE;  // Ensure max 32 characters for a 16x2 display
}

    // Copy data from user space
    if (copy_from_user(buffer, user_buffer, count))
        return -EFAULT;

    buffer_size = count;  // Store byte count
    buffer[count] = '\0';  // Null-terminate buffer

    // Replace newline with space
    if (buffer[count - 1] == '\n')
        buffer[count - 1] = ' ';

    // Write data to device
    write_full(buffer);
    pr_info("lcd_driver - Write. Update buffer content: %s\n", buffer);

    return count;  // Return number of bytes written
}

static long int my_ioctl(struct file *filep, unsigned cmd, unsigned long arg) {
    int user_input;
    struct lcd_pins usr_pins;

    switch (cmd) {
        case CLEAR:
            pr_info("lcd_driver - Clear display\n");
            lcd_byte(LCD_CLEAR_DISPLAY, 0);  // Clear display
            break;

        case LINE_1:
            // Copy user input for cursor position
            if (copy_from_user(&user_input, (int32_t __user *)arg, sizeof(user_input))) {
                pr_err("lcd_driver - Error setting ROW 1 cursor\n");
                return -EFAULT;
            }
            // Set cursor if valid position
            if (user_input >= 0 && user_input < 16) {
                lcd_byte(LCD_LINE_1 | user_input, 0);
                pr_info("lcd_driver - ROW 1 cursor set at col %d\n", user_input);
                msleep(10);
            } else {
                pr_err("lcd_driver - Out of screen bounds\n");
                return -EINVAL;
            }
            break;

        case LINE_2:
            // Copy user input for cursor position
            if (copy_from_user(&user_input, (int32_t __user *)arg, sizeof(user_input))) {
                pr_err("lcd_driver - Error setting ROW 2 cursor\n");
                return -EFAULT;
            }
            // Set cursor if valid position
            if (user_input >= 0 && user_input < 16) {
                lcd_byte(LCD_LINE_2 | user_input, 0);
                pr_info("lcd_driver - ROW 2 cursor set at col %d\n", user_input);
                msleep(10);
            } else {
                pr_err("lcd_driver - Out of screen bounds\n");
                return -EINVAL;
            }
            break;

        case SCROLL_LEFT:
            // Copy user input for scroll count
            if (copy_from_user(&user_input, (int32_t __user *)arg, sizeof(user_input))) {
                pr_err("lcd_driver - Error scrolling left\n");
                return -EFAULT;
            }
            // Scroll display left
            for (int i = 0; i < user_input; ++i) {
                lcd_byte(LCD_SCROLL_LEFT, 0);
                msleep(500);
            }
            pr_info("lcd_driver - Scroll left by %d\n", user_input);

            break;

        case SCROLL_RIGHT:
            // Copy user input for scroll count
            if (copy_from_user(&user_input, (int32_t __user *)arg, sizeof(user_input))) {
                pr_err("lcd_driver - Error scrolling right\n");
                return -EFAULT;
            }
            // Scroll display right
            for (int i = 0; i < user_input; ++i) {
                lcd_byte(LCD_SCROLL_RIGHT, 0);
                msleep(500);
            }
            pr_info("lcd_driver - Scroll right by %d\n", user_input);

            break;

        case INIT:
            // Copy user pin assignments and initialize LCD
            if (copy_from_user(&usr_pins, (struct lcd_pins __user *)arg, sizeof(usr_pins))) {
                pr_err("lcd_driver - Error copying user lcd pin assignments");
                return -EFAULT;
            } else {
                lcd_init(&usr_pins);
            }
            pr_info("lcd_driver - INIT. rs=%d, en=%d, d4 = %d, d5 = %d, d6 = %d, d7 = %d",usr_pins.rs,usr_pins.en,usr_pins.d4,usr_pins.d5,usr_pins.d6,usr_pins.d7);
            break;
        default:
            return -EINVAL;  // Invalid command
    }
    return 0;
}

static struct file_operations fops = {
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl,
};

static void pulseEnable(void) {
    gpiod_set_value(en, 1);
    usleep_range(2000, 5000);
    gpiod_set_value(en, 0);
    usleep_range(2000, 5000);
}

static void write4bits(uint8_t value) {
    for (int i = 0; i < 4; i++)
        gpiod_set_value(data_pins[i], (value >> i) & 0x01);
    pulseEnable();
}

static void lcd_byte(uint8_t bits, uint8_t mode) {
    gpiod_set_value(rs, mode);
    write4bits(bits >> 4);
    write4bits(bits & 0x0F);
}

static int write_full(char string[]) {
    gpiod_set_value(rs, 1);
    int i;
    for (i = 0; i < 32; i++) {
        if (string[i] == '\0')
            break;
        lcd_byte(string[i], 1);
    }
    msleep(10);
    return 0;
}

static int gpio_init(struct lcd_pins * pins) {
    rs = gpio_to_desc(IO_OFFSET + pins->rs);
    en = gpio_to_desc(IO_OFFSET + pins->en);
    data_pins[0] = gpio_to_desc(IO_OFFSET + pins->d4);
    data_pins[1] = gpio_to_desc(IO_OFFSET + pins->d5);
    data_pins[2] = gpio_to_desc(IO_OFFSET + pins->d6);
    data_pins[3] = gpio_to_desc(IO_OFFSET + pins->d7);

    if (!rs || !en || !data_pins[0] || !data_pins[1] || !data_pins[2] || !data_pins[3])
        return -EINVAL;

    gpiod_direction_output(rs, 0);
    gpiod_direction_output(en, 0);
    for (int i = 0; i < 4; i++)
        gpiod_direction_output(data_pins[i], 0);

    return 0;
}

static int lcd_init(struct lcd_pins * pins) {
    if (gpio_init(pins) < 0)
        return -1;

    msleep(50);
    lcd_byte(0x03, 0);
    msleep(5);
    lcd_byte(0x03, 0);
    msleep(5);
    lcd_byte(0x03, 0);
    msleep(5);
    lcd_byte(0x02, 0);

    lcd_byte(LCD_FUNCTION_SET, 0);
    lcd_byte(LCD_DISPLAY_OFF, 0);
    lcd_byte(LCD_CLEAR_DISPLAY, 0);
    lcd_byte(LCD_ENTRY_MODE_SET, 0);
    msleep(2);
    
    lcd_byte(0x0C,0);

    write_full("LCD initialized");

    return 0;
}

static int __init lcd_driver_init(void) {
    // Register the character device
    major = register_chrdev(0, "lcd_dev", &fops);
    if (major < 0) {
        pr_err("lcd_driver - Failed to register character device. Err: %d\n", major);
        return major;  // Return the error code from register_chrdev
    }
    
    pr_info("lcd_driver - Init. Major : %d\n", major);
    return 0;  // Successfully registered the device
}

static void __exit lcd_driver_exit(void) {
    // Unregister the character device
    unregister_chrdev(major, "lcd_dev");
    
    // Clear the LCD display
    lcd_byte(LCD_CLEAR_DISPLAY, 0);
    
    // Cleanup the GPIOs
    for (int i = 0; i < 4; i++) {
        if (data_pins[i])  // Check if the GPIO pin is initialized
            gpiod_set_value(data_pins[i], 0);
    }
    
    if (rs)  // Check if the RS pin is initialized
        gpiod_set_value(rs, 0);
    
    if (en)  // Check if the EN pin is initialized
        gpiod_set_value(en, 0);
    
    pr_info("lcd_driver - Exit. GPIO cleaned\n");
}

module_init(lcd_driver_init);
module_exit(lcd_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("1602A LCD Driver for Raspberry Pi 4B");
