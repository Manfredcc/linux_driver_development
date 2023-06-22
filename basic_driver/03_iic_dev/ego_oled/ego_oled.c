#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include "ego_oled.h"
#include "oled_lib.h"
#include "ssd1306.h"/*Tem*/

/*===================================================================================
                        SSD1306-IIC-OLED Drivers

GENERAL DESCRIPTION
    This driver which implements basic function for a oled-module compatible
with I2C bus.

    When            Who             What, Where, Why
    ----------      ---             -------------------------
    2023/06/11      Manfred         Initial release
    2023/06/13      Manfred         Fill basic framework
    2023/06/20      Manfred         Add ssd1306-lib and implement basic function
    2023/06/20      Manfred         Change ops method from Ioctrl to Sysfs

====================================================================================*/

pegoist chip = NULL;
#define POWER_OFF   0
#define POWER_ON    1

/* Implement OPS-Sysfs */
static ssize_t screen_power_store(struct class *c, struct class_attribute *attr, 
                    const char *buf, size_t count)
{
    int tmp = false;

    ego_info(chip, "Called\n");
    sscanf(buf, "%d", &tmp);
    if (true != tmp && false != tmp) {
        ego_err(chip, "Invalid input\n");
        return -EINVAL;
    }
    chip->oled.screen_on = tmp;
    oled_operation.FUNC->oled_power(chip, chip->oled.screen_on);

    ego_info(chip, "screen is %s\n", chip->oled.screen_on ? "On" : "OFF");
    return count;
}

static ssize_t screen_power_show(struct class *c, struct class_attribute *attr, char *buf)
{
    ego_info(chip, "Called\n");

    return scnprintf(buf, PAGE_SIZE, "screen:%s\n", chip->oled.screen_on ? "On" : "OFF");
}
CLASS_ATTR_RW(screen_power);

static ssize_t screen_clear_store(struct class *c, struct class_attribute *attr, 
                    const char *buf, size_t count)
{
    int tmp = false;

    ego_info(chip, "Called\n");
    sscanf(buf, "%d", &tmp);
    if (tmp == true) {
        oled_operation.FUNC->oled_clear();
        ego_info(chip, "screen is cleaned\n");

    } else {
        return -EINVAL;
    }
    return count;
}

static ssize_t screen_clear_show(struct class *c, struct class_attribute *attr, char *buf)
{
    ego_info(chip, "Called\n");

    return 0;
}
CLASS_ATTR_RW(screen_clear);


static struct attribute *egoist_class_attrs[] = {
    &class_attr_screen_power.attr,
    &class_attr_screen_clear.attr,
    NULL
};
ATTRIBUTE_GROUPS(egoist_class);

static int ego_open(struct inode *inode, struct file *filp)
{
    pegoist dev = container_of(inode->i_cdev, egoist, cdev);
    pr_err("call %s\n", __func__);
    if (!dev) {
        ego_err(chip, "container_of didn't found any valid data\n");
        return -ENODEV;
    }
    filp->private_data = dev;

    ego_info(dev, "Open\n");

    return 0;
}

static ssize_t ego_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    pegoist dev = (pegoist)filp->private_data;

    return copy_to_user(buf, &dev->oled.status, sizeof(int));
}

static ssize_t ego_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    static unsigned char val[10];

    pegoist dev = (pegoist)filp->private_data;

    if (copy_from_user(val, buf, count)) {
        ego_err(dev, "Kernel write failed!\n");
        return -EFAULT;
    }

    ego_err(dev, "*val = %d\n", val[0]);
    switch (val[0]) {
    case 0:
        oled_operation.FUNC->oled_power(chip, POWER_OFF);
        break;
    case 1:
        oled_operation.FUNC->oled_power(chip, POWER_ON);
        break;
    default:
        ego_info(dev, "Invalid input\n");
        return -EINVAL;
    }

    return count;
}

static int ego_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations ego_oled_ops = {
    .owner            =   THIS_MODULE,
    .open             =   ego_open,
    .read             =   ego_read,
    .write            =   ego_write,
    .release          =   ego_release,
};

static int ego_char_init(pegoist chip)
{
    int ret;

    ret = alloc_chrdev_region(&chip->devt, 0, 1, "ego_oled");
    if (ret) {
        ego_err(chip, "Failed to allocate a char device number for [ego_oled]\n");
        return EBUSY;
    }
    chip->major = MAJOR(chip->devt);
    ego_info(chip, "major = %d\n", chip->major);

    cdev_init(&chip->cdev, &ego_oled_ops);
    chip->cdev.owner = THIS_MODULE;

    cdev_add(&chip->cdev, chip->devt, 1);

    /* Initialize ego_class, visible in /sys/class */
    chip->class.name = "egoist_class";
    chip->class.owner = THIS_MODULE;
    chip->class.dev_groups = egoist_class_groups;
    ret = class_register(&chip->class);
    if (ret) {
        ego_err(chip, "Failed to create egoist class\n");
        unregister_chrdev_region(chip->devt, 1);

        return ret;
    }

    chip->dev = device_create(&chip->class, NULL,
                                    chip->devt, NULL,
                                    "%s", "egoist_oled");
    if (IS_ERR(&chip->dev)) {
        ego_err(chip, "Faile to create ego device\n");
        class_destroy(&chip->class);
        unregister_chrdev_region(chip->devt, 1);

        return PTR_ERR(chip->dev);
    }

    ego_info(chip, "egoist had been initialized successfully\n");
    return 0;
}

void probe_release(pegoist chip)
{
    ego_info(chip, "Enter\n");

    if (chip != NULL) {

        if (!IS_ERR_OR_NULL(chip->dev)) {
            unregister_chrdev_region(chip->devt, 1);
            device_destroy(&chip->class, chip->devt);
        }

        if (!IS_ERR_OR_NULL(&chip->class)) {
            unregister_chrdev_region(chip->devt, 1);
            cdev_del(&chip->cdev);
            class_destroy(&chip->class);
        }
       /* More resource will be added below */
    }
    ego_info(chip, "Out\n");
}

static int ego_oled_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    int ret = 0;

    pr_err("%s: Enter\n", __func__);

    do {
        if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
            ret = EIO;
            break;
        }

        chip = kzalloc(sizeof(*chip), GFP_KERNEL);
        if (!chip) {
            pr_err("Failed to alloc mem for ego_oled\n");
            ret = ENOMEM;
            break;
        }
        chip->name = "ego_oled";
        chip->info_option = true;
        chip->oled.status = false;
        chip->client = client;
        chip->regmap_config.reg_bits = 8;
        chip->regmap_config.val_bits = 8;
        chip->regmap = regmap_init_i2c(client, &chip->regmap_config);
        if (IS_ERR(chip->regmap)) {
            ret = PTR_ERR(chip->regmap);
            break;
        }

        switch (id->driver_data) {
        case SSD1306:
            oled_operation.ID = SSD1306;
            break;
        case SH1106:
            oled_operation.ID = SH1106;
            break;
        }
        ops_init(); /* Initialize oled-opearions for loaded chip */
        if (!oled_operation.initialized) {
            ego_err(chip, "Failed to initialized oled_operation\n");
            break;
        }

        ret = ego_char_init(chip);
        if (ret) {
            ego_err(chip, "egoist failed to initialize\n");
            break;
        }
        
        oled_operation.FUNC->oled_init(chip);
        ssd1306_display_string(0, 0, "Lucy:We'll meet again on the Moon!", 16, 1);
        oled_operation.FUNC->oled_refresh(chip);
    } while(0);

    if (ret) {
        ego_info(chip, "Egoist:Something goes wrong, holyshit!\n");
        probe_release(chip);
    }

    i2c_set_clientdata(client, chip);
    ego_info(chip, "Egoist:All things goes well, awesome!\n");

    return 0;   
}

static int ego_oled_remove(struct i2c_client *client)
{
    chip = (pegoist)i2c_get_clientdata(client);
    ego_info(chip, "Enter\n");

    oled_operation.FUNC->oled_power(chip, POWER_OFF);
    probe_release(chip);

    return 0;
}

static const struct i2c_device_id ego_oled_id[] = {
    {"ego-ssd1306", 0},
    {"ego-ssd1306", 1},
    { /* sentinel */}
};
MODULE_DEVICE_TABLE(i2c, ego_oled_id);

static struct i2c_driver ego_oled_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = "egoist",
    },
    .probe  = ego_oled_probe,
    .remove = ego_oled_remove,
    .id_table = ego_oled_id,
};

module_i2c_driver(ego_oled_i2c_driver);

MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implements basic function for a oled-module compatible with I2C bus");
MODULE_LICENSE("GPL");
