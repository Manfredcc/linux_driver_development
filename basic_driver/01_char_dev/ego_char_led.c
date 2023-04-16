#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

/*============================================================
            GENRATE A SIMPLE CHARACTER DEVICE

GENERAL DESCRIPTION
    This driver which implement a simple char device

    When              Who             What, Where, Why
    ----------        ---             ------------------------
    2023/04/08        Manfred         Initial reversion
    2023/04/16        Manfred         Modify for Imx6ull-led

==============================================================*/
static int count = 1;   /* Numbers of char devices */
module_param(count, int, S_IRUGO);

static bool debug_option = true;    /* hard-code control debugging */

#define ego_err(chip, fmt, ...)     \
    pr_err("%s: %s " fmt, chip->name,   \
        __func__, ##__VA_ARGS__)

#define ego_debug(chip, fmt, ...)   \
    do {                                        \
        if (debug_option)                       \
            pr_info("%s: %s: " fmt, chip->name,    \
                __func__, ##__VA_ARGS__);   \
        else                                \
            ;   \
    } while(0)


struct egoist {
    char *name;

    struct cdev cdev;
    struct class *ego_class;
    struct device *ego_device;
    int major;
    int minor;
    dev_t devt;
    char *dev_name;
    int gpio_num;   /* The serial number of GPIO for led to use */
    struct device_node *nd;

    bool debug_on;

    void *ego_data;
};
struct egoist *chip;

/* Implement OPS */
static int ego_open(struct inode *inode, struct file *filp)
{
    ego_debug(chip, "called %s\n", __func__);
    filp->private_data = chip;

    return 0;
}

static ssize_t ego_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    ego_debug(chip, "called\n");

    return count;
}

enum LED_STATUS {
    LED_OFF,
    LED_ON,
    STATUS_MAX,
};
static ssize_t ego_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
	int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;
	struct egoist *dev = filp->private_data;

	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LED_ON) {
		gpio_set_value(dev->gpio_num, 0);	/* 打开LED灯 */
	} else if(ledstat == LED_OFF) {
		gpio_set_value(dev->gpio_num, 1);	/* 关闭LED灯 */
	}
	return 0;
}

static int ego_release(struct inode *inode, struct file *filep)
{
    ego_debug(chip ,"called\n");

    return 0;
}

static const struct file_operations ego_ops = {
    .owner    =   THIS_MODULE,
    .open     =   ego_open,
    .read     =   ego_read,
    .write    =   ego_write,
    .release  =   ego_release,
};

static int __init ego_char_init(void)
{
    int ret;
    int cnt;

    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    if (!chip) {
        ego_debug(chip, "Failed to alloc mem for egoist[self_struct]\n");
        return ENOMEM;
    }
    chip->name = "Egoist";
    chip->debug_on = debug_option;

    /* Config GPIO -1- Get the gpio node*/
    chip->nd = of_find_node_by_path("/gpioled");
    if (!chip->nd) {
        ego_err(chip, "Not get gpioled node\n");
        return -EINVAL;
    }
    ego_debug(chip, "Find gpioled node\n");
    /* Config GPIO -2- Get the property of gpioled node for gpio_num */
    chip->gpio_num = of_get_named_gpio(chip->nd, "led-gpio", 0);
    if (chip->gpio_num < 0) {
        ego_err(chip, "Can't access the led-gpio");
        return -EINVAL;
    }
    ego_debug(chip, "led-gpio num = %d\n", chip->gpio_num);
    /* Config GPIO -3- Set the direction of the GPIO */
    ret = gpio_direction_output(chip->gpio_num, 1);
    if (ret < 0) {
        ego_err(chip, "Can't set gpio\n");
    }

    /*----------------------------------------------
    Step -1- Allocate a char device number for [cdev]
            Take the dynamic method API
                alloc_chrdev_region
    ------------------------------------------------*/
    ret = alloc_chrdev_region(&chip->devt, 0, count, "ego_char");
    if (ret) {
        ego_err(chip, "Failed to allocate a char device number for [ego_char]\n");
        return EBUSY;
    }

    chip->major = MAJOR(chip->devt);
    ego_debug(chip, "major = %d\n", chip->major);
    /*----------------------------------------------
    Step -2- Initialize [cdev] and associate it with
    the [File OPS]
            API:cdev_init
    ------------------------------------------------*/
    cdev_init(&chip->cdev, &ego_ops);
    chip->cdev.owner = THIS_MODULE;

    /*----------------------------------------------
    Step -3- Add the char-dev[ego_char] to the system
    the [File OPS]
            API:cdev_add
    ------------------------------------------------*/
    ret = cdev_add(&chip->cdev, chip->devt, count);
    if (ret) {
        ego_err(chip ,"Failed to add [ego_char] to system\n");
        return EBUSY;
    }

    /* Create a device class, visible in /sys/class */
    chip->ego_class = class_create(THIS_MODULE, "egoist");
    if (IS_ERR(chip->ego_class)) {
        ego_err(chip, "Failed to create egoist class\n");
        unregister_chrdev_region(chip->devt, count);
        
        return PTR_ERR(chip->ego_class);
    }

    /* Create a device, visible in /dev/ */
    chip->dev_name = "egoist_";
    for (cnt = 0; cnt < count; cnt++) {
        chip->ego_device = device_create(chip->ego_class, NULL,
                                        chip->devt + cnt, NULL,
                                        "%s%d", chip->dev_name, cnt);
        if (IS_ERR(&chip->ego_device)) {
            ego_err(chip, "Faile to create ego device\n");
            class_destroy(chip->ego_class);
            unregister_chrdev_region(chip->devt, count);
        
            return PTR_ERR(chip->ego_device);
        }
    }

    ego_debug(chip, "Egoist:Awesome!\n");

    return 0;
}

static void __exit ego_char_exit(void)
{
    int cnt;

    unregister_chrdev_region(chip->devt, count);
    for (cnt = 0; cnt < count; cnt++) {
        device_destroy(chip->ego_class, MKDEV(chip->major, cnt));
    }
    cdev_del(&chip->cdev);
    class_destroy(chip->ego_class);

    ego_debug(chip, "Egoist:See you someday!\n");
}

module_init(ego_char_init);
module_exit(ego_char_exit);

MODULE_PARM_DESC(count, "Numbers of char-dev that need to be register");
MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implement a simple char device");
MODULE_LICENSE("GPL");
