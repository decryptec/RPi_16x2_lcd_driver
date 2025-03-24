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
#define E_DELAY 2  // Enable pulse delay in milliseconds

// 4-bit Interface Data Pins
#define data_4 6
#define data_5 13
#define data_6 19
#define data_7 26

// LCD Control Commands
#define LCD_ENTRY_MODE_SET  0x06  // Cursor moves right, no display shift
#define LCD_DISPLAY_ON      0x0C  // Display ON, Cursor OFF, Blink OFF
#define LCD_CLEAR_DISPLAY   0x01  // Clear display
#define LCD_FUNCTION_SET    0x28  // 4-bit mode, 2-line, 5x7 font
#define LCD_HOME           0x02   // Return home

static struct gpio_desc *rs, *en, *d4, *d5, *d6, *d7;
static struct gpio_desc *data_pins[4];

static char buffer[32];  // LCD buffer (max 32 chars for 16x2 display)

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

// GPIO Cleanup
static void gpio_cleanup(void)
{
    gpiod_put(rs);
    gpiod_put(en);
    gpiod_put(d4);
    gpiod_put(d5);
    gpiod_put(d6);
    gpiod_put(d7);
    pr_info("lcd_driver - GPIOs cleaned up\n");
}

// Enable Pulse for LCD
static void pulseEnable(void)
{
    gpiod_set_value(en, 1);
    usleep_range(2000, 5000);  // Increase stability
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

// Clear and Initialize LCD
static int lcd_init(void)
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

    return 0;
}

// Write string to LCD
static int write_lcd(char string[])
{
    int i;
    gpiod_set_value(rs, 1);

    for (i = 0; string[i] != '\0' && i < 16; i++) {
        lcd_byte(string[i], 1);
    }

    return 0;
}

// Module Initialization
static int __init lcd_driver_init(void)
{
    lcd_init();
    write_lcd("Hello World!!!");
    return 0;
}

// Module Cleanup
static void __exit lcd_driver_exit(void)
{
    lcd_byte(LCD_CLEAR_DISPLAY, 0);
    gpio_cleanup();
}

module_init(lcd_driver_init);
module_exit(lcd_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("Fixed LCD Driver for Raspberry Pi 4B via char device");

