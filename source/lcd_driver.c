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
    if (*offset >= len)
        return 0;

    if (count < len - *offset)
        len = count;
    else
        len = len - *offset;

    if (copy_to_user(user_buffer, buffer + *offset, len))
        return -EFAULT;

    *offset += len;
    
    if(buffer>0)
    {
        lcd_byte(LCD_CLEAR_DISPLAY,0);
        lcd_byte(LCD_LINE_1,0);
        msleep(5);
        write_full(buffer);
    }
    return len;
}

static ssize_t my_write(struct file *filp, const char __user *user_buffer, size_t count, loff_t *offset) {
    if (count >= MAX_BUFFER_SIZE)
        count = MAX_BUFFER_SIZE - 1;

    if (copy_from_user(buffer, user_buffer, count))
        return -EFAULT;

    buffer_size = count;
    buffer[count] = '\0';
    if (buffer[count - 1] == '\n')
        buffer[count - 1] = ' ';

    write_full(buffer);
    return count;
}

static long int my_ioctl(struct file *filep, unsigned cmd, unsigned long arg) {
    int user_input;
    switch (cmd) {
        case CLEAR:
            lcd_byte(LCD_CLEAR_DISPLAY, 0);
            break;
        case LINE_1:
            lcd_byte(LCD_LINE_1, 0);
            break;
        case LINE_2:
            lcd_byte(LCD_LINE_2, 0);
            break;
        case SCROLL_LEFT:
            if (copy_from_user(&user_input, (int32_t __user *)arg, sizeof(user_input)))
                return -EFAULT;
            for (int i = 0; i < user_input; ++i) {
                lcd_byte(LCD_SCROLL_LEFT, 0);
                msleep(500);
            }
            break;
        case SCROLL_RIGHT:
            if (copy_from_user(&user_input, (int32_t __user *)arg, sizeof(user_input)))
                return -EFAULT;
            for (int i = 0; i < user_input; ++i) {
                lcd_byte(LCD_SCROLL_RIGHT, 0);
                msleep(500);
            }
            break;
        default:
            return -EINVAL;
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
    for (int i = 0; i < 32; i++) {
        if (string[i] == '\0')
            break;
        else if (i == 16)
            lcd_byte(LCD_LINE_2, 0);
        lcd_byte(string[i], 1);
    }
    return 0;
}

static int gpio_init(void) {
    rs = gpio_to_desc(IO_OFFSET + REG_SELECT);
    en = gpio_to_desc(IO_OFFSET + ENABLE);
    data_pins[0] = gpio_to_desc(IO_OFFSET + DATA_4);
    data_pins[1] = gpio_to_desc(IO_OFFSET + DATA_5);
    data_pins[2] = gpio_to_desc(IO_OFFSET + DATA_6);
    data_pins[3] = gpio_to_desc(IO_OFFSET + DATA_7);

    if (!rs || !en || !data_pins[0] || !data_pins[1] || !data_pins[2] || !data_pins[3])
        return -EINVAL;

    gpiod_direction_output(rs, 0);
    gpiod_direction_output(en, 0);
    for (int i = 0; i < 4; i++)
        gpiod_direction_output(data_pins[i], 0);

    return 0;
}

static int __init lcd_driver_init(void) {
    if (gpio_init() < 0)
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
    lcd_byte(LCD_DISPLAY_ON, 0);
    lcd_byte(LCD_ENTRY_MODE_SET, 0);
    lcd_byte(LCD_CLEAR_DISPLAY, 0);
    msleep(2);

    write_full("ioctl ver");

    major = register_chrdev(0, "lcd_dev", &fops);
    if (major < 0)
        return major;

    return 0;
}

static void __exit lcd_driver_exit(void) {
    unregister_chrdev(major, "lcd_dev");
    lcd_byte(LCD_CLEAR_DISPLAY, 0);
    for (int i = 0; i < 4; i++)
        gpiod_set_value(data_pins[i], 0);
    gpiod_set_value(rs, 0);
    gpiod_set_value(en, 0);
}

module_init(lcd_driver_init);
module_exit(lcd_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("Optimized LCD Driver for Raspberry Pi 4B");
