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

/*==========================================================================
                        SSD1306-IIC-OLED Drivers

GENERAL DESCRIPTION
    This driver which implements basic function for a oled-module compatible
with I2C bus.

    When            Who             What, Where, Why
    ----------      ---             -------------------------
    2023/06/11      Manfred         Initial release
    2023/06/13      Manfred         Fill basic framework

===========================================================================*/

pegoist chip = NULL;
struct ops *ops = NULL;

/*====================OPS for OLED==================+====*/
void ssd1306_clear(pegoist chip)
{
    return;
}

#define OLED_CMD    0x00
bool ssd1306_init(pegoist chip)
{
    int ret;

    if (!chip) {
        pr_err("Egoist not initialized\n");
        return -EINVAL;
    }

    /* Reset Oled */
    ret |= gpio_direction_output(chip->oled.gpio, 1);
    msleep(100);
    ret |= gpio_direction_output(chip->oled.gpio, 0);
    msleep(200);
    ret |= gpio_direction_output(chip->oled.gpio, 1);
    if (ret) {
        ego_err(chip, "Can't set oled-gpio[VCC]\n");
    }

    regmap_write(chip->regmap, OLED_CMD, 0xAE);
    regmap_write(chip->regmap, OLED_CMD, 0x00);
    regmap_write(chip->regmap, OLED_CMD, 0x10);
    regmap_write(chip->regmap, OLED_CMD, 0x40);
    regmap_write(chip->regmap, OLED_CMD, 0x81);
    regmap_write(chip->regmap, OLED_CMD, 0xCF);
    regmap_write(chip->regmap, OLED_CMD, 0xA1);
    regmap_write(chip->regmap, OLED_CMD, 0xC8);
    regmap_write(chip->regmap, OLED_CMD, 0xA6);
    regmap_write(chip->regmap, OLED_CMD, 0xA8);
    regmap_write(chip->regmap, OLED_CMD, 0x3f);
    regmap_write(chip->regmap, OLED_CMD, 0xD3);
    regmap_write(chip->regmap, OLED_CMD, 0x00);
    regmap_write(chip->regmap, OLED_CMD, 0xd5);
    regmap_write(chip->regmap, OLED_CMD, 0x80);
    regmap_write(chip->regmap, OLED_CMD, 0xD9);
    regmap_write(chip->regmap, OLED_CMD, 0xF1);
    regmap_write(chip->regmap, OLED_CMD, 0xDA);
    regmap_write(chip->regmap, OLED_CMD, 0x12);
    regmap_write(chip->regmap, OLED_CMD, 0xDB);
    regmap_write(chip->regmap, OLED_CMD, 0x40);
    regmap_write(chip->regmap, OLED_CMD, 0x20);
    regmap_write(chip->regmap, OLED_CMD, 0x02);
    regmap_write(chip->regmap, OLED_CMD, 0x8D);
    regmap_write(chip->regmap, OLED_CMD, 0x14);
    regmap_write(chip->regmap, OLED_CMD, 0xA4);
    regmap_write(chip->regmap, OLED_CMD, 0xA6);
    regmap_write(chip->regmap, OLED_CMD, 0xAF);
    ops->FUNC->oled_clear(chip);

    return 0;
}

enum OLED_MODE {
    COLOR_DISPLAY_NORMAL,
    COLOR_INVERT,
    DISPLAY_INVERT,
    COLOR_DISPLAY_INVERT,
};
int ssd1306_conf(pegoist chip, int mode)
{
    return 0;
}

bool ssd1306_refresh(pegoist chip)
{
    return 0;
}

bool ssd1306_power(pegoist chip, bool poweron)
{
    return 0;
}

/*========================================================*/
static OPS_LIB ops_lib[MAX_SLAVE_CHIP] = { /* standard ops for slave chip */
    {   /* SSD1306 */
        .oled_clear = ssd1306_clear,
        .oled_init = ssd1306_init,
        .oled_conf = ssd1306_conf,
        .oled_refresh = ssd1306_refresh,
        .oled_power = ssd1306_power,
    },

};

/* Implement OPS */
#define OPS_LIB     'E'
#define LIB_INIT_NO     0x01
#define LIB_CONF_NO     0x02
#define LIB_REFRESH_NO  0x03
#define LIB_POWER_NO    0x04

#define LIB_INIT    _IO(OPS_LIB, LIB_INIT_NO)
#define LIB_CONF    _IOW(OPS_LIB, LIB_CONF_NO, unsigned long)
#define LIB_REFRESH _IO(OPS_LIB, LIB_REFRESH_NO)
#define LIB_POWER   _IOW(OPS_LIB, LIB_POWER_NO, unsigned long)
static long ego_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case LIB_INIT:
        ops->FUNC->oled_init(chip);
        break;
    case LIB_CONF:
        ops->FUNC->oled_conf(chip, arg);
        break;
    case LIB_REFRESH:
        ops->FUNC->oled_refresh(chip);
        break;
    case LIB_POWER:
        ops->FUNC->oled_power(chip, arg);
        break;
    default:
        return -ENOTTY;
    }
    return 0;
}

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
    return 0;
}

static int ego_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations ego_oled_ops = {
    .owner            =   THIS_MODULE,
    .unlocked_ioctl   =   ego_ioctl,
    .open             =   ego_open,
    .read             =   ego_read,
    .write            =   ego_write,
    .release          =   ego_release,
};



static void ops_init(struct ops *ops)
{
    if (ops->initialized) {
        ego_info(chip, "Already initialized\n");
        return;
    }
    ops->ID = SSD1306; //TODO:Different configurations to load
    ops->FUNC = &ops_lib[SSD1306];
    ops->initialized = true;

    return;
}

#define ND_PARSE    "/ego_oled"
static int oled_parse(pegoist chip)
{
    chip->nd = of_find_node_by_path(ND_PARSE);
    if (!chip->nd) {
        ego_err(chip, "Failed to get node-info\n");
        return -EINVAL;
    }
    ego_info(chip, "find node-info successfully\n");

    chip->oled.gpio = of_get_named_gpio(chip->nd, "oled-gpio", 0);
    memset(chip->oled.name, 0, sizeof(chip->oled.name));
    sprintf(chip->oled.name, "oled");
    gpio_request(chip->oled.gpio, chip->oled.name);

    return 0;
}

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

    ret = cdev_add(&chip->cdev, chip->devt, 1);
    if (ret) {
        ego_err(chip ,"Failed to add [ego_oled] to system\n");
        return EBUSY;
    }

    /* Initialize ego_class, visible in /sys/class */
    chip->class.name = "egoist_class";
    chip->class.owner = THIS_MODULE;
    // chip->ego_class.dev_groups = egoist_class_groups;
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

void probe_release(pegoist)
{
    //TODO
}

static int ego_oled_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    int ret;

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
        chip->regmap_config.reg_bits = 8;
        chip->regmap_config.val_bits = 8;
        chip->regmap = regmap_init_i2c(client, &chip->regmap_config);
        if (IS_ERR(chip->regmap)) {
            ret = PTR_ERR(chip->regmap);
            break;
        }

        ops = (struct ops *)kzalloc(sizeof(*ops), GFP_KERNEL);
        if (!ops) {
            ego_err(chip, "Failed to alloc mem for oled-ops\n");
            ret = ENOMEM;
            break;
        }
        ops_init(ops);

        ret = oled_parse(chip);
        if (ret) {
            ego_err(chip, "Failed to parse oled info\n");
            break;
        }

        ret = ego_char_init(chip);
        if (ret) {
            ego_err(chip, "egoist failed to initialize\n");
            break;
        }
        
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
    return 0;
}

/* fit different matching styles */
static const struct of_device_id ego_oled_match[] = {
    { .compatible = "egoist, ssd1306", .data = (void *)0 },
    // { .compatible = "egoist, xxc", .data = (void *)1 },
    { /* sentinel */}
};

static const struct i2c_device_id ego_oled_id[] = {
    {"egoist, ssd1306", 0},
    // {"egoist, xxc", 1},
    { /* sentinel */}
};

static struct i2c_driver ego_oled_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = "ego_oled",
        .of_match_table = of_match_ptr(ego_oled_match),
    },
    .probe  = ego_oled_probe,
    .remove = ego_oled_remove,
    .id_table = ego_oled_id,
};

module_i2c_driver(ego_oled_i2c_driver);

MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implements basic function for a oled-module compatible with I2C bus");
MODULE_LICENSE("GPL");
