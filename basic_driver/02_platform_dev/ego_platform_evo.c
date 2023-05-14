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
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/platform_device.h>
#include <linux/input.h>

/*==================================================================================
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
    2023/05/03        Manfred         Add key-inter to control beep
    2023/05/04        Manfred         Add sysfs properties to debug beep
    2023/05/07        Manfred         Transplant to platform-drv
    2023/05/07        Manfred         Add input-subsys for key press

===================================================================================*/
static int count = 1;   /* Numbers of char devices - default:1 */
module_param(count, int, S_IRUGO);

static bool debug_option = true;    /* hard-code control debugging */

#define ego_err(chip, fmt, ...)     \
    pr_err("%s: %s " fmt, chip->name,   \
        __func__, ##__VA_ARGS__)

#define ego_info(chip, fmt, ...)   \
    do {                                        \
        if (debug_option)                       \
            pr_info("%s: %s: " fmt, chip->name,    \
                __func__, ##__VA_ARGS__);   \
        else                                \
            ;   \
    } while(0)

struct irq_keydesc {
    char name[10];
    int gpio;
    int irqnum;
    unsigned char value;
    irqreturn_t (*handler)(int, void *);
};

struct beep {
    char name[10];
    int gpio;
    bool status;
    int duration;
};

#define KEY_NUM     1
struct egoist {
    char *name;

    struct cdev cdev;
    struct class ego_class;
    struct device *ego_device;
    int major;
    int minor;
    dev_t devt;
    char *dev_name;
    struct beep beep;
    struct irq_keydesc irqkeydesc[KEY_NUM]; /* array of key interrupts */
    unsigned char current_keynum;
    struct input_dev *idev_key;
    atomic_t releasekey; /* indicate key has been pressed and released */

    struct device_node *nd;

    bool debug_on;

    /* synchronous protection */
    atomic_t lock;
    atomic_t keyvalue;

    struct timer_list debounce_timer;
    struct hrtimer beep_timer;

    void *ego_data;
};
struct egoist *chip;

static ssize_t duration_show(struct class *c, struct class_attribute *attr, char *buf)
{
    ego_info(chip ,"called\n");

    return scnprintf(buf, PAGE_SIZE, "beep:%d\n", chip->beep.duration);
}

static ssize_t duration_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count)
{
    ego_info(chip ,"called\n");

    sscanf(buf, "%d", &chip->beep.duration);
    ego_info(chip, "store %d to duration\n", chip->beep.duration);

    return count;
}

CLASS_ATTR_RW(duration);

static ssize_t activate_show(struct class *c, struct class_attribute *attr, char *buf)
{
    ego_info(chip ,"called\n");

    return scnprintf(buf, PAGE_SIZE, "beep:%d\n", chip->beep.status);
}

static ssize_t activate_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count)
{
    int activate = 0;
    ego_info(chip ,"called\n");

    sscanf(buf, "%d", &activate);
    gpio_direction_output(chip->beep.gpio, 0);
    if (1 == activate) {
        chip->beep.status = 1;
        hrtimer_start(&chip->beep_timer, ms_to_ktime(chip->beep.duration), HRTIMER_MODE_REL);
    }

    return count;
}

CLASS_ATTR_RW(activate);

static struct attribute *egoist_class_attrs[] = {
    &class_attr_duration.attr,
    &class_attr_activate.attr,
    NULL
};
ATTRIBUTE_GROUPS(egoist_class);

/**
 * Interrupt Service Routine
 * responsible for key-interrupt
*/
static irqreturn_t key0_handler(int irq, void *dev_id)
{
    struct egoist *dev = (struct egoist *)dev_id;

    dev->current_keynum = 0;
    dev->debounce_timer.data = (volatile long)dev_id;
    mod_timer(&dev->debounce_timer, jiffies + msecs_to_jiffies(10));
    return IRQ_RETVAL(IRQ_HANDLED);
}

void debounce_timer_function(unsigned long arg)
{
    unsigned char value;
    unsigned char num;
    struct irq_keydesc *keydesc;
    struct egoist *dev = (struct egoist *)arg;

    num = dev->current_keynum;
    keydesc = &dev->irqkeydesc[num];
    value = gpio_get_value(keydesc->gpio);
    if (0 == value) {
        // atomic_set(&dev->keyvalue, keydesc->value);
        input_report_key(chip->idev_key, keydesc->value, 1);
        input_sync(chip->idev_key);
        gpio_direction_output(chip->beep.gpio, 0);
        ego_info(chip, "Starting timer to fire in %dms (%ld)\n", \
           chip->beep.duration, jiffies );
        chip->beep.status = 1;
        hrtimer_start(&chip->beep_timer, ms_to_ktime(chip->beep.duration), HRTIMER_MODE_REL);
    } else {
        // atomic_set(&dev->keyvalue, 0x80 | keydesc->value);
        // atomic_set(&dev->releasekey, 1);
        input_report_key(chip->idev_key, keydesc->value, 0);
        input_sync(chip->idev_key);
    }
}

enum hrtimer_restart beep_duration_hrtimer(struct hrtimer *timer)
{
    ego_info(chip, "%s called (%ld).\n", __func__, jiffies);
    gpio_direction_output(chip->beep.gpio, 1);
    chip->beep.status = 0;

    return HRTIMER_NORESTART;
}

/* Implement OPS */
static int ego_open(struct inode *inode, struct file *filp)
{
    ego_info(chip, "called %s\n", __func__);
    if (!atomic_dec_and_test(&chip->lock)) {
        ego_info(chip, "busy\n");
        atomic_inc(&chip->lock);
        return -EBUSY;
    }

    filp->private_data = chip;
    ego_info(chip, "end call %s\n", __func__);

    return 0;
}

#define VALID_KEY   0x01
#define INVALID_KEY 0x00
static ssize_t ego_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    int ret = 0;
    unsigned char keyvalue = 0;
    unsigned char releasekey = 0;
    struct egoist *dev = (struct egoist *)filp->private_data;
    // ego_info(chip, "called\n");

    keyvalue = atomic_read(&dev->keyvalue);
    releasekey = atomic_read(&dev->releasekey);

    if (releasekey) { /* there's a button pressed */
        if (keyvalue & 0x80) {
            keyvalue &= ~0x80;
            ret = copy_to_user(buf, &keyvalue, sizeof(keyvalue));
            gpio_direction_output(chip->beep.gpio, 0);
            ego_info(chip, "Starting timer to fire in %dms (%ld)\n", \
           chip->beep.duration, jiffies );
            chip->beep.status = 1;
            hrtimer_start(&chip->beep_timer, ms_to_ktime(chip->beep.duration), HRTIMER_MODE_REL);
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
    ego_info(chip ,"called\n");

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

static int beep_init(struct egoist *chip)
{
    int ret;

    chip->beep.duration = 1000; /* unit:ms */
    chip->beep.status = 0;      /* not active */

    chip->nd = of_find_node_by_path("/egoist/beep");
    if (!chip->nd) {
        ego_err(chip, "Not get bepp node\n");
        return -EINVAL;
    }
    ego_info(chip, "find beep node successfully\n");

    chip->beep.gpio = of_get_named_gpio(chip->nd, "beep-gpio", 0);
    memset(chip->beep.name, 0, sizeof(chip->beep.name));
    sprintf(chip->beep.name, "beep");
    gpio_request(chip->beep.gpio, chip->beep.name);
    ret = gpio_direction_output(chip->beep.gpio, 1);
    if (ret) {
            ego_err(chip, "Can't set beep-gpio\n");
    }

    hrtimer_init(&chip->beep_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    chip->beep_timer.function = beep_duration_hrtimer;

    return 0;
}

static int key_init(struct egoist *chip)
{
    int i;
    int ret;

    /* -1- Get the key node*/
    chip->nd = of_find_node_by_path("/egoist/key");
    if (!chip->nd) {
        ego_err(chip, "Not get key node\n");
        return -EINVAL;
    }
    ego_info(chip, "find key node successfully\n");

    /* -2- Get the gpio_num of key */
    for (i = 0; i < KEY_NUM; i++) {
        chip->irqkeydesc[i].gpio = of_get_named_gpio(chip->nd, "key-gpio", i);
        if (chip->irqkeydesc[i].gpio < 0) {
            ego_err(chip, "Can't access the key-gpio%d\n", i);
            return -EINVAL;
        }
        ego_info(chip, "key-gpio num = %d\n", chip->irqkeydesc[i].gpio);
    }

    /* -3- Initialize the GPIOs corresponding to KEY-array, and set interrupts for them */
    for (i = 0; i < KEY_NUM; i++) {
        memset(chip->irqkeydesc[i].name, 0, sizeof(chip->irqkeydesc[i].name));
        sprintf(chip->irqkeydesc[i].name, "KEY%d", i);
        gpio_request(chip->irqkeydesc[i].gpio, chip->irqkeydesc[i].name);
        ret = gpio_direction_input(chip->irqkeydesc[i].gpio);
        if (ret) {
            ego_err(chip, "Can't set key-gpio\n");
        }
        /* get the irq-line from fdt */
        chip->irqkeydesc[i].irqnum = irq_of_parse_and_map(chip->nd, i);
        ego_info(chip, "key%d [gpio_num:%d, irq_num:%d]\n", i, 
                                    chip->irqkeydesc[i].gpio, 
                                    chip->irqkeydesc[i].irqnum);
    }

    /* request irq */
    chip->irqkeydesc[0].handler = key0_handler;
    chip->irqkeydesc[0].value = KEY_0;

    for (i = 0; i < KEY_NUM; i++) {
        ret = request_irq(chip->irqkeydesc[i].irqnum,
                          chip->irqkeydesc[i].handler,
                          IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
                          chip->irqkeydesc[i].name,
                          chip);
        if (ret < 0) {
            ego_info(chip, "irq request %d failed!\n", i);
            return -EFAULT;
        }
    }

    /* request input_dev */
    chip->idev_key = input_allocate_device();
    if (!chip->idev_key) {
        ego_err(chip, "create input device failed\n");
        return -ENOMEM;
    }
    chip->idev_key->name = "key_input";
    /* set to keystork event, repeatable */
    chip->idev_key->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
    /* initialize key info */
    input_set_capability(chip->idev_key, EV_KEY, KEY_0);

    ret = input_register_device(chip->idev_key);
    if (ret) {
        ego_err(chip, "register input key failed\n");
        return ret;
    }

    return 0;
}

static int egoist_probe(struct platform_device *dev)
{
    int ret;
    int cnt;

    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    if (!chip) {
        ego_info(chip, "Failed to alloc mem for egoist[self_struct]\n");
        return ENOMEM;
    }
    chip->name = "Egoist";
    chip->debug_on = debug_option;
    atomic_set(&chip->lock, 1);
    atomic_set(&chip->keyvalue, INVALID_KEY);
    atomic_set(&chip->releasekey, 0);

    key_init(chip); /* initialize key resources : gpio.inter */
    beep_init(chip);

    /* create a timer for debounce */
    init_timer(&chip->debounce_timer);
    chip->debounce_timer.function = debounce_timer_function;

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
    ego_info(chip, "major = %d\n", chip->major);
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

    /* Initialize ego_class, visible in /sys/class */
    chip->ego_class.name = "egoist";
    chip->ego_class.owner = THIS_MODULE;
    chip->ego_class.dev_groups = egoist_class_groups;
    ret = class_register(&chip->ego_class);
    if (ret) {
        ego_err(chip, "Failed to create egoist class\n");
        unregister_chrdev_region(chip->devt, count);

        return ret;
    }

    /* Create a device, visible in /dev/ */
    chip->dev_name = "egoist_";
    for (cnt = 0; cnt < count; cnt++) {
        chip->ego_device = device_create(&chip->ego_class, NULL,
                                        chip->devt + cnt, NULL,
                                        "%s%d", chip->dev_name, cnt);
        if (IS_ERR(&chip->ego_device)) {
            ego_err(chip, "Faile to create ego device\n");
            class_destroy(&chip->ego_class);
            unregister_chrdev_region(chip->devt, count);

            return PTR_ERR(chip->ego_device);
        }
    }

    ego_info(chip, "Egoist:Awesome!\n");

    return 0;
}

static int egoist_remove(struct platform_device *dev)
{
    int cnt;
    int ret;

    del_timer_sync(&chip->debounce_timer);
    ret = hrtimer_cancel(&chip->beep_timer);
    if (ret) {
        ego_info(chip, "the beep timer is still in use\n");
    }

    for (cnt = 0; cnt < KEY_NUM; cnt++) {
        free_irq(chip->irqkeydesc[cnt].irqnum, chip);
        gpio_free(chip->irqkeydesc[cnt].gpio);
    }
    gpio_free(chip->beep.gpio);

    input_unregister_device(chip->idev_key);
    input_free_device(chip->idev_key);

    unregister_chrdev_region(chip->devt, count);
    for (cnt = 0; cnt < count; cnt++) {
        device_destroy(&chip->ego_class, MKDEV(chip->major, cnt));
    }
    cdev_del(&chip->cdev);
    class_destroy(&chip->ego_class);

    ego_info(chip, "Egoist:See you someday!\n");

    return 0;
}

static const struct of_device_id ego_of_match[] = {
    {.compatible = "Egoist forever"},
    {/* Sentinel */},
};

static struct platform_driver egoist_drv = {
    .probe         = egoist_probe,
    .remove        = egoist_remove,
    .driver        = {
        .name      = "Egoist_drv",
        .owner     = THIS_MODULE,
        .of_match_table = ego_of_match,
    }
};
module_platform_driver(egoist_drv);

MODULE_PARM_DESC(count, "Numbers of char-dev that need to be register");
MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implement a simple char device");
MODULE_LICENSE("GPL");
