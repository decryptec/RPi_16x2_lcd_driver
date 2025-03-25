#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define IO_OFFSET 512
#define MAX_BUFFER_SIZE 32
//Control pins
#define reg_select 0
#define enable 5

// 4-bit Interface Data Pins
#define data_4 6
#define data_5 13
#define data_6 19
#define data_7 26

// LCD Configuration
#define N 1 // Number of lines (HIGH = 2, LOW = 1)
#define F 0 // Character font (HIGH = 5x11 dots, LOW = 5x8 dots)

// LCD Control Commands
#define LCD_ENTRY_MODE_SET  0x06  // Cursor moves right, no display shift
#define LCD_DISPLAY_ON      0x0C  // Display ON, Cursor OFF, Blink OFF
#define LCD_CLEAR_DISPLAY   0x01  // Clear display
#define LCD_FUNCTION_SET    0x28  // 4-bit mode, 2-line, 5x7 font
#define LCD_LINE_1 0x80
#define LCD_LINE_2 0xC0

static struct gpio_desc *rs, *en, *d4, *d5, *d6, *d7;
static struct gpio_desc *data_pins[4];

static char buffer[MAX_BUFFER_SIZE];  // LCD buffer (max 32 chars for 16x2 display)
static ssize_t buffer_size = 0;
static int major = 0;

static int write_line(char string[],char line);
static int write_full(char string[]);

static ssize_t my_read (struct file *filp, char __user *user_buffer, size_t count, loff_t *offset){
    size_t len = buffer_size; // Use the size of the data in buffer
    int result;

    // Check if offset exceeds the length of the buffer (EOF condition)
    if (*offset >= len) {
        return 0; // Return 0 to indicate EOF
    }

    // Adjust `len` if the user requests fewer bytes than available
    if (count < len - *offset) {
        len = count;
    } else {
        len = len - *offset;
    }

    // Copy data from the kernel buffer to the user buffer
    result = copy_to_user(user_buffer, buffer + *offset, len);
    if (result != 0) {
        pr_err("lcd_driver - Failed to copy data to user space.\n");
        return -EFAULT; // Error code for failure
    }

    *offset += len; // Update the file offset

    pr_info ("lcd_driver - my_read: Sent %zu bytes: %s\n", len, buffer);
    return len; // Return the number of bytes read
}

static ssize_t my_write (struct file *filp, const char __user *user_buffer, size_t count, loff_t *offset){
	// Ensure we don't overflow the buffer
    if (count > MAX_BUFFER_SIZE) {
        printk(KERN_WARNING "Input exceeds buffer size. Data will be truncated.\n");
        count = MAX_BUFFER_SIZE; // Truncate to fit buffer
    }

    // Copy data from user space to kernel space
    if (copy_from_user(buffer, user_buffer, count) != 0) {
        printk(KERN_ERR "Failed to copy data from user space.\n");
        return -EFAULT; // Error code for failed copy
    }

    buffer_size = count; // Update the buffer size
    if (count > 0)
        buffer[count-1] = '\0'; // Null-terminate the string

    pr_info("lcd_driver - my_write: Stored %zu bytes: %s\n", buffer_size, buffer);
    write_full(buffer);
    return count; // Return the number of bytes written
}

static struct file_operations fops = {
	.read = my_read,
	.write = my_write
};

// Enable Pulse for LCD
static void pulseEnable(void)
{
    gpiod_set_value(en, 1);
    usleep_range(2000, 5000);
    gpiod_set_value(en, 0);
    usleep_range(2000, 5000);
}

// Write 4-bit Data
static void write4bits(uint8_t value)
{
    for (int i = 0; i < 4; i++) {
        gpiod_set_value(data_pins[i], (value >> i) & 0x01);  // Send LSB first
    }
    pulseEnable();
}

// Write command or data byte
static void lcd_byte(uint8_t bits, uint8_t mode)
{
    gpiod_set_value(rs, mode);

    // Send high nibble
    write4bits(bits >> 4);
    // Send low nibble
    write4bits(bits & 0x0F);
}

// Write string to LCD
static int write_line(char string[],char line)
{
    int i;
    gpiod_set_value(rs, 1);

    if (line < 0 || line > 1) {
        pr_err("lcd_driver - Invalid line number\n");
        return -1;
    }
    
    switch (line) {
        case 0:
            lcd_byte(LCD_LINE_1, 0);
            break;
        case 1:
            lcd_byte(LCD_LINE_2, 0);
            break;
    }

    for (i = 0; string[i] != '\0' && i < 16; i++) {
        lcd_byte(string[i], 1);
    }

    return 0;
}

static int write_full(char string[])
{
	int i;
    lcd_byte(LCD_CLEAR_DISPLAY, 0);  // Clear display
	gpiod_set_value(rs, 1);

	lcd_byte(LCD_LINE_1, 0);
	for (i = 0;  i < MAX_BUFFER_SIZE-1; i++) {
		if(string[i]=='\0')
			break;
		else if(i==15){
			msleep(5);
			lcd_byte(LCD_LINE_2,0);
		}
		lcd_byte(string[i], 1);

	}

	return 0;
}

// GPIO Initialization
int gpio_init(void) {
    int status = 0;

    rs = gpio_to_desc(IO_OFFSET + reg_select);
    en = gpio_to_desc(IO_OFFSET + enable);
    d4 = gpio_to_desc(IO_OFFSET + data_4);
    d5 = gpio_to_desc(IO_OFFSET + data_5);
    d6 = gpio_to_desc(IO_OFFSET + data_6);
    d7 = gpio_to_desc(IO_OFFSET + data_7);

    if (!rs || !en || !d4 || !d5 || !d6 || !d7) {
        pr_err("lcd_driver - Error getting GPIOs\n");
        return -EINVAL;
    }

    data_pins[0] = d4;
    data_pins[1] = d5;
    data_pins[2] = d6;
    data_pins[3] = d7;

    status = gpiod_direction_output(rs, 0);
    status |= gpiod_direction_output(en, 0);
    for (int i = 0; i < 4; i++) {
        status |= gpiod_direction_output(data_pins[i], 0);
    }

    if (status) {
        pr_err("lcd_driver - GPIO output setup failed\n");
        return status;
    }

    pr_info("lcd_driver - GPIOs initialized successfully\n");
    return 0;
}

// Module Initialization
static int __init lcd_driver_init(void)
{
    gpio_init();

    msleep(50);  // Power-on delay

    lcd_byte(0x03, 0);  // Initialize in 8-bit mode
    lcd_byte(0x02, 0);  // Switch to 4-bit mode
    lcd_byte(LCD_FUNCTION_SET, 0);  // 4-bit, 2-line, 5x7 font
    lcd_byte(LCD_DISPLAY_ON, 0);  // Display ON, Cursor OFF
    lcd_byte(LCD_ENTRY_MODE_SET, 0);  // Auto Increment cursor
    lcd_byte(LCD_CLEAR_DISPLAY, 0);  // Clear display
    msleep(2);

    write_full("FOPS ver: Hello World!!!");

    major = register_chrdev(0, "lcd_dev", &fops); 
	if (major < 0) {
		pr_info("lcd_driver - error registering chrdev\n");
		return major;
	}
	pr_info("lcd_driver - Major Device Number: %d\n", major);

    return 0;
}

// Module Cleanup
static void __exit lcd_driver_exit(void)
{
    unregister_chrdev(major, "lcd_dev");
    pr_info("lcd_driver - lcd_dev unregistered\n");
    lcd_byte(LCD_CLEAR_DISPLAY, 0);
    gpiod_set_value(rs, 0);
    gpiod_set_value(en, 0);
    gpiod_set_value(d4, 0);
    gpiod_set_value(d5, 0);
    gpiod_set_value(d6, 0);
    gpiod_set_value(d7, 0);
    pr_info("lcd_driver - Exit success: screen cleared and GPIOs cleaned\n");
}

module_init(lcd_driver_init);
module_exit(lcd_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("Fixed LCD Driver for Raspberry Pi 4B via char device");


