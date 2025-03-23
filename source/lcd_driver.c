#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

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
    for (int i = 0; i < 4; i++) {
        status = gpiod_direction_output(data_pins[i], 0);
        if (status) {
            pr_err("lcd_driver - Error setting data pin %d to output\n", i);
            return status;
        }
    }

    pr_info("lcd_driver - GPIOs initialized successfully\n");
    return 0;
}

// GPIO Cleanup
static void gpio_cleanup(void)
{
    rs = en = d4 = d5 = d6 = d7 = NULL;
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

// LCD Initialization
static int lcd_init(void)
{
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

    return 0;
}

// Module Initialization
static int __init my_init(void)
{
    major = register_chrdev(0, "lcd_cdev", &fops);
    if (major < 0) {
        pr_err("lcd_driver - Error registering chrdev\n");
        return major;
    }

    if (gpio_init()) {
        unregister_chrdev(major, "lcd_cdev");
        pr_err("lcd_driver - GPIO initialization failed\n");
        return -EINVAL;
    }

    lcd_init();  // Initialize LCD after GPIO setup

    pr_info("lcd_driver - Major Device Number: %d\n", major);
    return 0;
}

// Module Exit
static void __exit my_exit(void)
{
    gpio_cleanup();
    unregister_chrdev(major, "lcd_cdev");
    pr_info("lcd_driver - Module exited\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("Simple LCD Driver for Raspberry Pi 4B via char device");
