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
    XX,
    MAX_SLAVE_CHIP = XX
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

typedef bool (*OLED_INIT)(pegoist chip);
typedef int  (*OLED_CONF)(pegoist chip, int mode);
typedef bool (*OLED_REFRESH)(pegoist chip);
typedef bool (*OLED_POWER)(pegoist chip, bool poweron);

typedef struct _OPS_LIB {
    OLED_INIT       oled_init;
    OLED_CONF       oled_conf;
    OLED_REFRESH    oled_refresh;
    OLED_POWER      oled_power;
}OPS_LIB;

struct ops {
    bool initialized;
    OPS_LIB *FUNC;
    enum SLAVE_CHIP ID;
};

#endif /* __EGO_OLED__ */
