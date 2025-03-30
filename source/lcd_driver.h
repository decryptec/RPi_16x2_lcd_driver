#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <linux/ioctl.h>

#define IO_OFFSET 512
#define MAX_BUFFER_SIZE 33  // 16 chars + null char

// LCD Configuration
#define LCD_CLEAR_DISPLAY  0x01
#define LCD_FUNCTION_SET   0x28 // 4-bit, 2-line, 5x7 font
#define LCD_DISPLAY_OFF     0x08 // Display OFF, Cursor OFF, Blink OFF
#define LCD_DISPLAY_ON	0x0C
#define LCD_ENTRY_MODE_SET 0x06 // Auto Increment cursor
#define LCD_LINE_1         0x80
#define LCD_LINE_2         0xC0
#define LCD_SCROLL_LEFT    0x18
#define LCD_SCROLL_RIGHT   0x1C

struct lcd_pins{
	int rs;
	int en;
	int d4;
	int d5;
	int d6;
	int d7;
};

// ioctl commands
#define CLEAR         _IO('a', 'b')
#define LINE_1        _IOW('a', 'c', int32_t *)
#define LINE_2        _IOW('a', 'd', int32_t *)
#define SCROLL_LEFT   _IOW('a', 'e', int32_t *)
#define SCROLL_RIGHT  _IOW('a', 'f', int32_t *)
#define INIT _IOW('a', 'g', struct lcd_pins *)

static int lcd_init(struct lcd_pins * pins);
static void lcd_byte(uint8_t bits, uint8_t mode);
static int write_full(char string[]);

#endif // LCD_DRIVER_H

