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
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

/*============================================================
            GENRATE A SIMPLE CHARACTER DEVICE

GENERAL DESCRIPTION
    This driver which implement a simple char device

    When              Who             What, Where, Why
    ----------        ---             ------------------------
    2023/04/08        Manfred         Initial reversion
    2023/04/16        Manfred         Modify for Imx6ull-led
    2023/04/27        Manfred         Modify for Imx6ull-beep
    2023/05/01        Manfred         Add synchronous protection
    2023/05/02        Manfred         Simple key detection
    2023/05/02        Manfred         Add SPI for key input

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

struct irq_keydesc {
    int gpio;
    int irqnum;
    unsigned char value;
    char name[10];
    irqreturn_t (*handler)(int, void *);
};

#define KEY_NUM     1
struct egoist {
    char *name;

    struct cdev cdev;
    struct class *ego_class;
    struct device *ego_device;
    int major;
    int minor;
    dev_t devt;
    char *dev_name;
    // Move to irq_keydesc
    // int gpio_num;   /* The serial number of GPIO for led to use */
    struct irq_keydesc irqkeydesc[KEY_NUM]; /* array of key interrupts */
    unsigned char current_keynum;
    atomic_t releasekey;

    struct device_node *nd;

    bool debug_on;

    /* synchronous protection */
    atomic_t lock;
    atomic_t keyvalue;

    struct timer_list debounce_timer;

    void *ego_data;
};
struct egoist *chip;

/**
 * Interrupt Service Routine
 * responsible for key-interrupt
*/
static irqreturn_t key0_handler(int irq, void *dev_id)
{
    struct egoist *dev = (struct egoist *)dev_id;
    ego_debug(chip, "1\n");

    dev->current_keynum = 0;
    dev->debounce_timer.data = (volatile long)dev_id;
    mod_timer(&dev->debounce_timer, jiffies + msecs_to_jiffies(10));
    ego_debug(chip, "2\n");
    return IRQ_RETVAL(IRQ_HANDLED);
}

void timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct egoist *dev = (struct egoist *)arg;

    num = dev->current_keynum;
    keydesc = &dev->irqkeydesc[num];
    value = gpio_get_value(keydesc->gpio);
    if (0 == value) {
        atomic_set(&dev->keyvalue, keydesc->value);
    } else {
        atomic_set(&dev->keyvalue, 0x80 | keydesc->value);
        atomic_set(&dev->releasekey, 1);
    }
}

/* Implement OPS */
static int ego_open(struct inode *inode, struct file *filp)
{
    ego_debug(chip, "called %s\n", __func__);
    if (!atomic_dec_and_test(&chip->lock)) {
        ego_debug(chip, "busy\n");
        atomic_inc(&chip->lock);
        return -EBUSY;
    }

    filp->private_data = chip;
    ego_debug(chip, "end call %s\n", __func__);

    return 0;
}

#define VALID_KEY   0x01
#define INVALID_KEY 0xFF
static ssize_t ego_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    int ret = 0;
    unsigned char keyvalue = 0;
    unsigned char releasekey = 0;
    struct egoist *dev = (struct egoist *)filp->private_data;
    // ego_debug(chip, "called\n");

    keyvalue = atomic_read(&dev->keyvalue);
    releasekey = atomic_read(&dev->releasekey);

    if (releasekey) { /* there's a button pressed */
        if (keyvalue & 0x80) {
            keyvalue &= ~0x80;
            ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
        } else {
            goto data_error;
        }
        atomic_set(&dev->releasekey, 0);
    } else {
        goto data_error;
    }

    return 0;

data_error:
    return -EINVAL;
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
	// struct egoist *dev = filp->private_data;

	retvalue = copy_from_user(databuf, buf, count);
	if(retvalue < 0) {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	// if(ledstat == LED_ON) {
	// 	gpio_set_value(dev->gpio_num, 0);	/* 打开LED灯 */
	// } else if(ledstat == LED_OFF) {
	// 	gpio_set_value(dev->gpio_num, 1);	/* 关闭LED灯 */
	// }
	return 0;
}

static int ego_release(struct inode *inode, struct file *filp)
{
    struct egoist *chip = filp->private_data;
    ego_debug(chip ,"called\n");

    atomic_inc(&chip->lock);

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
    int i;

    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    if (!chip) {
        ego_debug(chip, "Failed to alloc mem for egoist[self_struct]\n");
        return ENOMEM;
    }
    chip->name = "Egoist";
    chip->debug_on = debug_option;
    atomic_set(&chip->lock, 1);
    atomic_set(&chip->keyvalue, INVALID_KEY);
    atomic_set(&chip->releasekey, 0);

    /* Config GPIO -1- Get the gpio node*/
    chip->nd = of_find_node_by_path("/key");
    if (!chip->nd) {
        ego_err(chip, "Not get key node\n");
        return -EINVAL;
    }
    ego_debug(chip, "Find key node\n");
    /* Config GPIO -2- Get the property of gpioled node for gpio_num */
    for (i = 0; i < KEY_NUM; i++) {
        chip->irqkeydesc[i].gpio = of_get_named_gpio(chip->nd, "key-gpio", i);
        if (chip->irqkeydesc[i].gpio < 0) {
            ego_err(chip, "Can't access the key-gpio%d\n", i);
            return -EINVAL;
        }
        ego_debug(chip, "key-gpio num = %d\n", chip->irqkeydesc[i].gpio);
    }
    /* Config GPIO -3- Initialize GPIOs of the KEY-array, and set them for interrupt */
    for (i = 0; i < KEY_NUM; i++) {
        memset(chip->irqkeydesc[i].name, 0, sizeof(chip->irqkeydesc[i].name));
        sprintf(chip->irqkeydesc[i].name, "KEY%d", i);
        gpio_request(chip->irqkeydesc[i].gpio, chip->irqkeydesc[i].name);
        ret = gpio_direction_input(chip->irqkeydesc[i].gpio);
        if (ret) {
            ego_err(chip, "Can't set gpio\n");
        }
        /* get the irq-line from fdt */
        chip->irqkeydesc[i].irqnum = irq_of_parse_and_map(chip->nd, i);
        ego_debug(chip, "key%d [gpio_num:%d, irq_num:%d]\n", i, 
                                    chip->irqkeydesc[i].gpio, 
                                    chip->irqkeydesc[i].irqnum);
    }
    /* request irq */
    chip->irqkeydesc[0].handler = key0_handler;
    chip->irqkeydesc[0].value = VALID_KEY;

    for (i = 0; i < KEY_NUM; i++) {
        ret = request_irq(chip->irqkeydesc[i].irqnum,
                          chip->irqkeydesc[i].handler,
                          IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                          chip->irqkeydesc[i].name,
                          chip);
        if (ret < 0) {
            ego_debug(chip, "irq request %d failed!\n", i);
            return -EFAULT;
        }
    }

    /* create a timer for debounce */
    init_timer(&chip->debounce_timer);
    chip->debounce_timer.function = timer_function;

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
