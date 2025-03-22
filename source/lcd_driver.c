#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>

static struct gpio_desc *lcd, *button;

//GPIO pins
#define VEE 5 //Controls contrast
#define reg_select 6
#define read_write 13
#define enable 19

//4-Wire Mode
#define data_0 12
#define data_1 16
#define data_2 20
#define data_3 21

//LED backlight
#define led_pos 7
#define led_neg 1

static int major = 0;

static ssize_t my_read (struct file *filp, char __user *user_buf, size_t len, loff_t *offset)
{
	
}

static ssize_t my_write (struct file *filp, const char __user *user_buf, size_t len, loff_t *offset)
{

}

static long my_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	
}

static struct file_operations fops = {
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl
};

static int __init my_init(void)
{
	major = register_chrdev(0, "lcd_cdev", &fops); 
	if (major < 0) {
		pr_info("lcd_driver - error registering chrdev\n");
		return major;
	}
	pr_info("lcd_driver - Major Device Number: %d\n", major);
	return 0;
}

static void __exit my_exit(void)
{
	unregister_chrdev(major, "lcd_dev");
	pr_info("lcd_driver - Exit\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Decryptec");
MODULE_DESCRIPTION("Simple LCD Driver for Raspberry Pi 4B via char device");
