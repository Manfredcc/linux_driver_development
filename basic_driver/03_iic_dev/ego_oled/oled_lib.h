#ifndef __OLED_LIB_H__
#define __OLED_LIB_H__

#include "ego_oled.h"

typedef bool (*OLED_INIT)(pegoist chip);
typedef int  (*OLED_CONF)(pegoist chip, int mode);
typedef bool (*OLED_REFRESH)(pegoist chip);
typedef bool (*OLED_POWER)(pegoist chip, bool poweron);
typedef void (*OLED_CLEAR)(pegoist chip);

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

/* prototype */
void ops_init(void);
void ssd1306_clear(pegoist chip);
bool ssd1306_init(pegoist chip);
int ssd1306_conf(pegoist chip, int mode);
bool ssd1306_refresh(pegoist chip);
bool ssd1306_power(pegoist chip, bool poweron);

#endif /* __OLED_LIB_H__ */