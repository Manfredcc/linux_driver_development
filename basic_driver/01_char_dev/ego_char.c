#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

/*============================================================
            GENRATE A SIMPLE CHARACTER DEVICE

GENERAL DESCRIPTION
    This driver which implement a simple char device

    When              Who             What, Where, Why
    ----------        ---             ------------------------
    2023/04/08        Manfred         Initial reversion

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

    bool debug_on;

    void *ego_data;
};
static struct egoist *chip;

/* Implement OPS */
static int ego_open(struct inode *inode, struct file *filep)
{
    ego_debug(chip, "called %s\n", __func__);

    return 0;
}

static ssize_t ego_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    ego_debug(chip, "called\n");

    return count;
}

static ssize_t ego_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    ego_debug(chip, "called\n");

    return count;
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
