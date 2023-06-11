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

/*==========================================================================
                        SSD1306-IIC-OLED Drivers

GENERAL DESCRIPTION
    This driver which implements basic function for a oled-module compatible
with I2C bus.

    When            Who             What, Where, Why
    ----------      ---             -------------------------
    2023/06/11      Manfred         Initial release

===========================================================================*/
/* Hardware info */
#define SSD1306_ADDR    0x78    /* I2C slave address */

/* Debug info option */
#define ego_err(chip, fmt, ...)     \
    pr_err("%s: %s " fmt, chip->name,   \
        __func__, ##__VA_ARGS__)

#define ego_info(chip, fmt, ...)    \
    do {                            \
        if(chip->info_option)       \
            pr_info("%s: %s " fmt, chip->name,  \
                __func__, ##__VA_ARGS__)        \
        else                                    \
            ;   \
    } while(0)

/* Self-dev-info */
enum OLED_MODE {
    COLOR_DISPLAY_NORMAL,
    COLOR_INVERT,
    DISPLAY_INVERT,
    COLOR_DISPLAY_INVERT,
};

enum SLAVE_CHIP {
    SSD1306,
    FIRST_SLAVE_CHIP = SSD1306,
    XX,
    MAX_SLAVE_CHIP = XX
};

typedef bool (*OLED_INIT)(void);
typedef int  (*OLED_CONF)(enum OLED_MODE mode);
typedef bool (*OLED_REFRESH)(void);

typedef struct _OPS_LIB {
    OLED_INIT       oled_init;
    OLED_CONF       oled_conf;
    OLED_REFRESH    oled_refresh;
}OPS_LIB;
static OPS_LIB ops_lib[MAX_SLAVE_CHIP]; /* standard ops for slave chip */

struct ops_lib {
    bool initialized;
    OPS_LIB (*oled_ops)[MAX_SLAVE_CHIP];
    enum SLAVE_CHIP ID;
};


typedef struct _egoist {
    char *name;
    struct device *dev;
    struct device_node *nd;
    struct class class;
    struct cdev cdev;
    dev_t  devt;
    int major;
    int minor;
    struct i2c_client *client;
    struct regmap *regmap;
    struct regmap_config regmap_config;

    bool info_option;

    struct ops_lib ops;
    
    void *private_data;
}egoist, *pegoist;
pegoist chip = NULL;


static int ego_oled_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
    return 0;   
}

static int ego_oled_remove(struct i2c_client *client)
{
    return 0;
}

/* fit different matching styles */
static const struct of_device_id ego_oled_match[] = {
    { .compatible = "egoist, ssd1306" },
    { .compatible = "egoist, xxc" },
    { /* sentinel */}
};

static const struct i2c_device_id ego_oled_id[] = {
    {"egoist, ssd1306", 0},
    {"egoist, xxc", 0},
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