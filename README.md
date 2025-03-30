# RPi_16x2_LCD_Driver

## **1. Install Dependencies**  
Before building the driver, ensure your system is updated and required packages are installed:  
```sh
sudo apt update && sudo apt upgrade -y
sudo apt install -y raspberrypi-kernel-headers build-essential gpiod
```

## **2. Build & Load the Module**  
Navigate to the source directory, compile, and load the module:  
```sh
make
dmesg -W  # Check kernel logs in another terminal
sudo insmod lcd_driver.ko

```
Create a device node (replace `236` with the major number from `dmesg` logs):  
```sh
sudo mknod /dev/lcd0 c 236 0
sudo chmod 777 /dev/lcd0
```

## **3. Unload the Module**  
To remove the module from the kernel:  
```sh
sudo rmmod lcd_driver
```

## **Finding GPIO Offsets**  
List available GPIO chips:  
```sh
gpiodetect
```
Determine GPIO offsets:  
```sh
cd /sys/class/gpio/
ls
cat /sys/class/gpio/*chip*/label
```
This will list available GPIO chips and their offsets, needed for configuring your LCD pins.

