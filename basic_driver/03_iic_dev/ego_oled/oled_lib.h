#ifndef __OLED_LIB_H__
#define __OLED_LIB_H__

#include "ego_oled.h"

enum OLED_MODE {
    COLOR_NORMAL,
    COLOR_INVERT,
    DISPLAY_NORMAL,
    DISPLAY_INVERT,
};

typedef void (*OLED_INIT)(pegoist chip);
typedef int  (*OLED_CONF)(pegoist chip, enum OLED_MODE mode);
typedef void (*OLED_REFRESH)(pegoist chip);
typedef bool (*OLED_POWER)(pegoist chip, bool poweron);
typedef void (*OLED_CLEAR)(void);

typedef struct _OPS_LIB {
    OLED_INIT       oled_init;
    OLED_CONF       oled_conf;
    OLED_REFRESH    oled_refresh;
    OLED_POWER      oled_power;
    OLED_CLEAR      oled_clear;
}OPS_LIB;

struct ops {
    bool initialized;
    OPS_LIB *FUNC;
    enum SLAVE_CHIP ID;
    char chip_name[20];
};
extern struct ops oled_operation;

void ops_init(void);

/* Chip::ssd1306-ops */
void ego_ssd1306_clear(void);
void ego_ssd1306_init(pegoist chip);
int ego_ssd1306_conf(pegoist chip, enum OLED_MODE mode);
void ego_ssd1306_refresh(pegoist chip);
bool ego_ssd1306_power(pegoist chip, bool poweron);


#endif /* __OLED_LIB_H__ */