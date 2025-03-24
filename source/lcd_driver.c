#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#define IO_OFFSET 512
#define reg_select 0
#define enable 5

// LCD Configuration
#define N 1 // Number of lines (HIGH = 2, LOW = 1)
#define F 0 // Character font (HIGH = 5x11 dots, LOW = 5x8 dots)

// 4-bit Interface Data Pins
#define data_4 6
#define data_5 13
#define data_6 19
#define data_7 26

// LCD Control Commands
#define LCD_ENTRY_MODE_SET  0x06  // Cursor moves right, no display shift
#define LCD_DISPLAY_ON      0x0C  // Display ON, Cursor OFF, Blink OFF
#define LCD_DISPLAY_OFF     0x08  // Display OFF
#define LCD_CLEAR_DISPLAY   0x01  // Clear display
#define LCD_FUNCTION_SET    0x20  // 4-bit mode, N=lines, F=font
#define LCD_HOME           0x02   // Return home (cursor to 0,0)

static struct gpio_desc *rs, *en, *d4, *d5, *d6, *d7;
static struct gpio_desc *data_pins[4];

static int major = 0;
static char buffer[32];  // LCD buffer (max 32 chars for 16x2 display)

// File Operations Stubs
static ssize_t my_read(struct file *filp, char __user *user_buf, size_t len, loff_t *offset)
{
    pr_info("lcd_driver - read() called\n");
    return -ENOSYS; // Function not implemented
}

static ssize_t my_write(struct file *filp, const char __user *user_buf, size_t len, loff_t *offset)
{
    pr_info("lcd_driver - write() called\n");
    return -ENOSYS; // Function not implemented
}

static long my_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    pr_info("lcd_driver - ioctl() called\n");
    return -ENOSYS; // Function not implemented
}

static struct file_operations fops = {
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl
};

// GPIO Initialization
int gpio_init(void) {
    int status = 0;

    // Request GPIOs
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

    // Assign GPIO descriptors to array
    data_pins[0] = d4;
    data_pins[1] = d5;
    data_pins[2] = d6;
    data_pins[3] = d7;

    // Set GPIOs to output
    status = gpiod_direction_ouput(rs,0);
        if (status) {
            pr_err("lcd_driver - Error setting data pin %d to output\n", i);t
            return status;
        }
    status = gpiod_direction_output(en,0);
        if (status) {
            pr_err("lcd_driver - Error setting data pin %d to output\n", i);t
            return status;
        }
    for (int i = 0; i < 4; i++) {
        status = gpiod_direction_output(data_pins[i], 0);
        if (status) {
            pr_err("lcd_driver - Error setting data pin %d to output\n", i);t
            return status;
        }
    }

    pr_info("lcd_driver - GPIOs initialized successfully\n");
    return 0;
}

// GPIO Cleanup
static void gpio_cleanup(void)
{
    gpiod_put(rs);
    gpiod_put(en);
    gpiod_put(d4);
    gpiod_put(d5);
    gpiod_put(d6);
    gpiod_put(d7);
    unregister_chrdev(major, "lcd_dev");
    pr_info("lcd_driver - GPIOs cleaned up\n");
}


// Enable Pulse for LCD
static void pulseEnable(void)
{
    gpiod_set_value(en, 0);
    udelay(1);
    gpiod_set_value(en, 1);
    udelay(1);  // Enable pulse >450 ns
    gpiod_set_value(en, 0);
    usleep_range(40, 100); // Commands need >37 us to settle
}

// Write 4-bit Data
static void write4bits(uint8_t value)
{
    for (int i = 0; i < 4; ++i) {
        gpiod_set_value(data_pins[3 - i], (value >> i) & 0x01);  // Send MSB first
    }
    pulseEnable();  // Ensure the LCD latches the data
}

// Copy string to buffer and determine length
static void str_to_buff(char input[], int *len)
{
    *len = 0; // Initialize length counter

    for (int i = 0; i < 31; ++i) { // Leave space for null terminator
        buffer[i] = input[i];
        
        if (input[i] == '\0') 
            break;  // Stop at null character

        (*len)++;  // Increment length
    }

    buffer[*len] = '\0'; // Ensure null termination
}

// Write string to LCD
static int write_lcd(char string[])  
{
    int len = 0;
    str_to_buff(string, &len);  // Copy and get actual length

    if (len > 32) {
        pr_err("lcd_driver - buffer overload\n");
        return -1;
    }

    // Set RS high to write
    gpiod_set_value(rs, 1);

    for (int i = 0; i < len; ++i) {
        write4bits(buffer[i] >> 4);  // Send upper nibble
        write4bits(buffer[i] & 0x0F);  // Send lower nibble

        if (buffer[i] == '\0')  // Stop at null terminator
            return 0;
        
        if (i == 15) { // Move cursor to second line
            write4bits(0xC0 >> 4);  // DDRAM address for row 2
            write4bits(0xC0 & 0x0F);
        }
    }

    return 0;
}

// LCD Initialization
static int lcd_init(void)
{

    major = register_chrdev(0, "lcd_dev", &fops); 
    if (major < 0) {
        printk("lcd_driver - error registering chrdev\n");
        return major;
    }
    pr_info("lcd_driver - Major Device Number: %d\n", major);

    gpio_init();

    msleep(50);  // Step 1: Power-on delay (>40ms after Vcc rises)

    // Step 2: Send Function Set (8-bit mode, repeated 3 times)
    write4bits(0x03);
    msleep(5);

    write4bits(0x03);
    udelay(150);

    write4bits(0x03);
    udelay(150);

    // Step 3: Set to 4-bit mode
    write4bits(0x02);

    // Step 4: Function Set (4-bit mode, N=lines, F=font)
    write4bits((LCD_FUNCTION_SET | (N << 3) | (F << 2)) >> 4);
    write4bits((LCD_FUNCTION_SET | (N << 3) | (F << 2)) & 0x0F);

    // Step 5: Display OFF
    write4bits(LCD_DISPLAY_OFF >> 4);
    write4bits(LCD_DISPLAY_OFF & 0x0F);

    // Step 6: Clear Display
    write4bits(LCD_CLEAR_DISPLAY >> 4);
    write4bits(LCD_CLEAR_DISPLAY & 0x0F);
    msleep(2);  // >1.52ms delay for clear display

    // Step 7: Entry Mode Set
    write4bits(LCD_ENTRY_MODE_SET >> 4);
    write4bits(LCD_ENTRY_MODE_SET & 0x0F);

    // Step 8: Display ON
    write4bits(LCD_DISPLAY_ON >> 4);
    write4bits(LCD_DISPLAY_ON & 0x0F);

    //LCD print
    write_lcd("Hello World");
    return 0;
}

module_init(lcd_init);
module_exit(gpio_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("Simple LCD Driver for Raspberry Pi 4B via char device");
