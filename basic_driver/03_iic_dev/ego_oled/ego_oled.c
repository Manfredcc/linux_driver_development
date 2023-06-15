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

/* Implement OPS */
#define OPS_LIB     'E'
#define LIB_CLEAR_NO    0x00
#define LIB_INIT_NO     0x01
#define LIB_CONF_NO     0x02
#define LIB_REFRESH_NO  0x03
#define LIB_POWER_NO    0x04

#define LIB_CLEAR   _IO(OPS_LIB, LIB_CLEAR_NO)
#define LIB_INIT    _IO(OPS_LIB, LIB_INIT_NO)
#define LIB_CONF    _IOW(OPS_LIB, LIB_CONF_NO, unsigned long)
#define LIB_REFRESH _IO(OPS_LIB, LIB_REFRESH_NO)
#define LIB_POWER   _IOW(OPS_LIB, LIB_POWER_NO, unsigned long)
static long ego_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {
    case LIB_CLEAR:
        oled_operation.FUNC->oled_clear(chip);
        break;
    case LIB_INIT:
        oled_operation.FUNC->oled_init(chip);
        break;
    case LIB_CONF:
        oled_operation.FUNC->oled_conf(chip, arg);
        break;
    case LIB_REFRESH:
        oled_operation.FUNC->oled_refresh(chip);
        break;
    case LIB_POWER:
        oled_operation.FUNC->oled_power(chip, arg);
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

void probe_release(pegoist chip)
{
    ego_info(chip, "Enter\n");

    if (chip != NULL) {
        if (chip->oled.gpio != -1) {
            gpio_free(chip->oled.gpio);
            chip->oled.gpio = -1;
        }

        if (!IS_ERR_OR_NULL(chip->class)) {
            unregister_chrdev_region(chip->devt, 1);
        }

        if (!IS_ERR_OR_NULL(chip->dev)) {
            unregister_chrdev_region(chip->devt, 1);
        }
       /* More resource will be added below */
    }
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

        switch (id->driver_data) {
        case SSD1306:
            oled_operation.ID = SSD1306;
        case SH1106:
            oled_operation.ID = SH1106;
        }
        ops_init(); /* Initialize oled-opearions for loaded chip */
        if (!oled_operation.initialized) {
            ego_err(chip, "Failed to initialized oled_operation\n");
            break;
        }

        chip->oled.gpio = -1;
        // ret = oled_parse(chip); //TODO 暂时使用固定VCC
        // if (ret) {
        //     ego_err(chip, "Failed to parse oled info\n");
        //     break;
        // }

        ret = ego_char_init(chip);
        if (ret) {
            ego_err(chip, "egoist failed to initialize\n");
            break;
        }
        
        oled_operation.FUNC->oled_init(chip);
        oled_operation.FUNC->oled_conf(chip, COLOR_DISPLAY_NORMAL);
        oled_draw_line();
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

    probe_release(chip);
}

static const struct i2c_device_id ego_oled_id[] = {
    {"egoist, ssd1306", 0},
    {"egoist, sh1106", 1},
    { /* sentinel */}
};

static struct i2c_driver ego_oled_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = "ego_oled",
    },
    .probe  = ego_oled_probe,
    .remove = ego_oled_remove,
    .id_table = ego_oled_id,
};

module_i2c_driver(ego_oled_i2c_driver);

MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implements basic function for a oled-module compatible with I2C bus");
MODULE_LICENSE("GPL");
