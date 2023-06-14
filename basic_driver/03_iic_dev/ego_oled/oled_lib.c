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
#include "oled_lib.h"

struct ops oled_operation;
static OPS_LIB ops_lib[MAX_SLAVE_CHIP] = { /* standard ops for slave chip */
    {   /* SSD1306 */
        .oled_clear = ssd1306_clear,
        .oled_init = ssd1306_init,
        .oled_conf = ssd1306_conf,
        .oled_refresh = ssd1306_refresh,
        .oled_power = ssd1306_power,
    },

    // {   /* SH1106 */
    //     .oled_clear = NULL,
    //     .oled_init = NULL,
    //     .oled_conf = NULL,
    //     .oled_refresh = NULL,
    //     .oled_power = NULL,
    // },
};

void ops_init(void)
{
    if (oled_operation.initialized) {
        pr_err("Already initialized\n");
        return;
    }

    do {
        switch (oled_operation.ID) {
        case SSD1306:
            oled_operation.FUNC = &ops_lib[SSD1306];
            strcpy(oled_operation.chip_name, "SSD1306");
            break;
        case SH1106:
            oled_operation.FUNC = &ops_lib[SH1106];
            strcpy(oled_operation.chip_name, "SH1106");
            break;
        default:
            oled_operation.initialized = false;
            strcpy(oled_operation.chip_name, "NotSupport");
            pr_err("Not support chip-ID\n");
            return;
        }
    } while(0);

    oled_operation.initialized = true;
    pr_info("%s: Support-ID:%s\n", __func__, oled_operation.chip_name);

    return;
}

/* SSD1306 */
/*==================================================================*/
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
    oled_operation.FUNC->oled_clear(chip);

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

MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implements basic function for a oled-module compatible with I2C bus");
MODULE_LICENSE("GPL");

/*==================================================================*/