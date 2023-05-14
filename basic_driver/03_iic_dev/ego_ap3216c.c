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

/*==================================================================================
                GENRATE A I2C DRIVER FOR AP3216C

GENERAL DESCRIPTION
    This driver which implements basic functions for device-ap3216c

    When              Who             What, Where, Why
    ----------        ---             ------------------------
    2023/05/13        Manfred         Initial release
    2023/05/14        Manfred         Use Regmap insteal of i2c_transfer

===================================================================================*/

#define AP3216C_ADDR    	0X1E	/* I2C slave address */
/* Register defines */
#define AP3216C_SYSTEMCONG	0x00
#define AP3216C_INTSTATUS	0X01
#define AP3216C_INTCLEAR	0X02
#define AP3216C_IRDATALOW	0x0A
#define AP3216C_IRDATAHIGH	0x0B
#define AP3216C_ALSDATALOW	0x0C
#define AP3216C_ALSDATAHIGH	0X0D
#define AP3216C_PSDATALOW	0X0E
#define AP3216C_PSDATAHIGH	0X0F

static bool debug_option = true;    /* hard-code control debugging */
#define ego_err(chip, fmt, ...)     \
    pr_err("%s: %s " fmt, chip->name,   \
        __func__, ##__VA_ARGS__)

#define ego_info(chip, fmt, ...)   \
    do {                                        \
        if (debug_option)                       \
            pr_info("%s: %s " fmt, chip->name,  \
                __func__, ##__VA_ARGS__);       \
        else                                    \
            ;   \
    } while(0)


typedef struct _egoist {
    char *name;
    struct device *ego_device;
    struct device_node *nd;
    struct class ego_class;
    struct cdev cdev;
    dev_t  devt;
    int major;
    int minor;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_config regmap_config;
    unsigned ir, als, ps;   /* ap3216c data */

    bool debug_on;
    
    void *private_data;
}egoist, *pegoist;
pegoist chip = NULL;

/*
 * Read value from specified reg
 * @dev:ap3216c-dev
 * @reg:register address
 * @buf:data buffer
 * @len:data number
 */
static unsigned char ap3216c_read_regs(pegoist dev, u8 reg)
{
    u8 ret;
    unsigned int data;

    ret = regmap_read(dev->regmap, reg, &data);
    return (u8)data;
}

/*
 * Write value to specified reg
 * @dev:ap3216c-dev
 * @reg:register address
 * @buf:data buffer
 * @len:data number
 */
static void ap3216c_write_regs(pegoist dev, u8 reg, u8 data)
{
    regmap_write(dev->regmap, reg, data);
}

/* Implement OPS */
static int ego_open(struct inode *inode, struct file *filp)
{
    pegoist dev = container_of(inode->i_cdev, egoist, cdev);
    pr_err("call %s\n", __func__);
    if (!dev) {
        ego_err(chip, "container_of didn't found any valid data\n");
        return -ENODEV;
    }
    filp->private_data = dev;

    /* initialize ap3216c-dev */
    ap3216c_write_regs(dev, AP3216C_SYSTEMCONG, 0x04);
    mdelay(50);
    ap3216c_write_regs(dev, AP3216C_SYSTEMCONG, 0x03);

    ego_info(dev, "initialize ap3216c-dev successfully!\n");

    return 0;
}

static ssize_t ego_read(struct file *filp, char __user *buf, size_t count, loff_t *offset)
{
    int cnt;
    short result[3];
    pegoist dev = (pegoist)filp->private_data;

    for(cnt = 0; cnt < 6; cnt++) { /* get raw data */
        buf[cnt] = ap3216c_read_regs(dev, AP3216C_IRDATALOW + cnt);
    }

    /* handle data [ir] */
    if (buf[0] & 0x80) { /* invalid data */
        dev->ir = 0;
    } else {
		dev->ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0X03); 			
    }

    /* handle data [als] */
    dev->als = ((unsigned short)buf[3] << 8) | buf[2];

    /* handle data [ps] */
    if (buf[4] & 0x40) {
        dev->ps = 0;
    } else {
		dev->ps = ((unsigned short)(buf[5] & 0X3F) << 4) | (buf[4] & 0X0F); 
    }

    result[0] = dev->ir;
    result[1] = dev->als;
    result[2] = dev->ps;

    return copy_to_user(buf, result, sizeof(result));
}

static ssize_t ego_write(struct file *filp, const char __user *buf, size_t count, loff_t *offset)
{
    // Todo
    return 0;
}

static int ego_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations ego_ap3216c_ops = {
    .owner    =   THIS_MODULE,
    .open     =   ego_open,
    .read     =   ego_read,
    .write    =   ego_write,
    .release  =   ego_release,
};

/*
 * Initial egoist as a char-dev
 * @chip: struct _egoist * chip
 */
static int ego_char_init(pegoist chip)
{
    int ret;

    ret = alloc_chrdev_region(&chip->devt, 0, 1, "ego_ap3216c");
    if (ret) {
        ego_err(chip, "Failed to allocate a char device number for [ego_ap3216c]\n");
        return EBUSY;
    }
    chip->major = MAJOR(chip->devt);
    ego_info(chip, "major = %d\n", chip->major);

    cdev_init(&chip->cdev, &ego_ap3216c_ops);
    chip->cdev.owner = THIS_MODULE;

    ret = cdev_add(&chip->cdev, chip->devt, 1);
    if (ret) {
        ego_err(chip ,"Failed to add [ego_ap3216c] to system\n");
        return EBUSY;
    }

    /* Initialize ego_class, visible in /sys/class */
    chip->ego_class.name = "egoist_class";
    chip->ego_class.owner = THIS_MODULE;
    // chip->ego_class.dev_groups = egoist_class_groups;
    ret = class_register(&chip->ego_class);
    if (ret) {
        ego_err(chip, "Failed to create egoist class\n");
        unregister_chrdev_region(chip->devt, 1);

        return ret;
    }

    chip->ego_device = device_create(&chip->ego_class, NULL,
                                    chip->devt, NULL,
                                    "%s", "egoist");
    if (IS_ERR(&chip->ego_device)) {
        ego_err(chip, "Faile to create ego device\n");
        class_destroy(&chip->ego_class);
        unregister_chrdev_region(chip->devt, 1);

        return PTR_ERR(chip->ego_device);
    }

    ego_info(chip, "egoist had been initialized successfully\n");
    return 0;
}

static int ego_ap3216c_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    int ret;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        return -EIO;

    chip = kzalloc(sizeof(*chip), GFP_KERNEL);
    if (!chip) {
        pr_err("Failed to alloc mem for egoist\n");
        return ENOMEM;
    }
    chip->name = "egoist";
    chip->debug_on = debug_option;
    chip->client = client;
    chip->regmap_config.reg_bits = 8;
    chip->regmap_config.val_bits = 8;
    chip->regmap = regmap_init_i2c(client, &chip->regmap_config);
    if (IS_ERR(chip->regmap)) {
        return PTR_ERR(chip->regmap);
    }

    ret = ego_char_init(chip);
    if (ret) {
        ego_err(chip, "egoist failed to initialize\n");
        return ret;
    }
    

    i2c_set_clientdata(client, chip);

    ego_info(chip, "Egoist:All things goes well, awesome!\n");
    return 0;
}

static int ego_ap3216c_remove(struct i2c_client *client)
{
    pegoist chip = i2c_get_clientdata(client);
    
    unregister_chrdev_region(chip->devt, 1);
    device_destroy(&chip->ego_class, chip->devt);
    cdev_del(&chip->cdev);
    class_destroy(&chip->ego_class);

    ego_info(chip, "Egoist:See you someday!\n");
    return 0;
}

/* fit different matching styles */
static const struct of_device_id ego_ap3216c_match[] = {
    { .compatible = "egoist, ap3216c" },
    { /* sentinel */}
};

static const struct i2c_device_id ego_ap3216c_id[] = {
    {"egoist, ap3216c", 0},
    { /* sentinel */}
};

static struct i2c_driver ego_ap3216c_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name  = "ego_ap3216c",
        .of_match_table = of_match_ptr(ego_ap3216c_match),
    },
    .probe  = ego_ap3216c_probe,
    .remove = ego_ap3216c_remove,
    .id_table = ego_ap3216c_id,
};

module_i2c_driver(ego_ap3216c_i2c_driver);

MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implements basic function for i2c-device-ap3216c ");
MODULE_LICENSE("GPL");