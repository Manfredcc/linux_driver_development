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
#include "ssd1306.h"

struct ops oled_operation;
static OPS_LIB ops_lib[MAX_SLAVE_CHIP] = { /* standard ops for slave chip */
    {   /* SSD1306 */
        .oled_init = ego_ssd1306_init,
        .oled_clear = ego_ssd1306_clear,
        .oled_conf = ego_ssd1306_conf,
        .oled_refresh = ego_ssd1306_refresh,
        .oled_power = ego_ssd1306_power,
    },

    // {   /* SH1106 */
    //     .oled_init = NULL,
    //     .oled_clear = NULL,
    //     .oled_conf = NULL,
    //     .oled_refresh = NULL,
    //     .oled_power = NULL,
    // },
};

/* Choose the proper chip-ops */
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

/* SSD1306 Operations */
void ego_ssd1306_clear(pegoist chip)
{
    ssd1306_clear_screen(0xff);
    ssd1306_clear_screen(0x00);
}

void ego_ssd1306_init(pegoist chip)
{
    set_ssd1306_i2c_client(chip->client);
    ssd1306_init();
    chip->oled.status = true;
}

int ego_ssd1306_conf(pegoist chip, enum OLED_MODE mode)
{
    ego_info(chip, "Enter\n");

    switch (mode) {
    case COLOR_NORMAL:
        regmap_write(chip->regmap, 0, 0xA6);
        break;
    case COLOR_INVERT:
        regmap_write(chip->regmap, 0, 0xA7);
        break;
    case DISPLAY_NORMAL:
        regmap_write(chip->regmap, 0, 0xC8);
        regmap_write(chip->regmap, 0, 0xA1);
        break;
    case DISPLAY_INVERT:
        regmap_write(chip->regmap, 0, 0xC0);
        regmap_write(chip->regmap, 0, 0xA0);
        break;
    }
    return 0;
}

void ego_ssd1306_refresh(pegoist chip)
{
    ssd1306_refresh_gram();
}

bool ego_ssd1306_power(pegoist chip, bool poweron)
{
    ego_info(chip, "Enter\n");
    
    if (poweron && !chip->oled.status) { /* Enable oled */
        ssd1306_display_on();
        chip->oled.status = true;
    }

    if (!poweron && chip->oled.status) { /* Disable oled */
        ssd1306_display_off();
        chip->oled.status = false;
    }

    return 0;
}

/*==================================================================*/

MODULE_AUTHOR("Manfred <1259106665@qq.com>");
MODULE_DESCRIPTION("This driver which implements basic function for a oled-module compatible with I2C bus");
MODULE_LICENSE("GPL");

