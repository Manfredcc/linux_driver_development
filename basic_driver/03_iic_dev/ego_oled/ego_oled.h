#ifndef __EGO_OLED__
#define __EGO_OLED__

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
                __func__, ##__VA_ARGS__);       \
        else                                    \
            ;   \
    } while(0)

/* Self-dev-info */

enum SLAVE_CHIP {
    SSD1306,
    FIRST_SLAVE_CHIP = SSD1306,
    SH1106,
    MAX_SLAVE_CHIP = SH1106
};

struct _oled {
    char name[10];
    int gpio;
    bool status;
};

typedef struct _egoist { /* For oled-dev */
    char *name;
    struct device *dev;
    struct device_node *nd;
    struct class class;
    struct cdev cdev;
    dev_t  devt;
    int major;
    int minor;
    struct _oled oled;
    struct regmap *regmap;
    struct regmap_config regmap_config;

    bool info_option;

    void *private_data;
}egoist, *pegoist;

#endif /* __EGO_OLED__ */
