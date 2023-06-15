#ifndef __OLED_LIB_H__
#define __OLED_LIB_H__

#include "ego_oled.h"

enum OLED_MODE {
    COLOR_DISPLAY_NORMAL,
    COLOR_INVERT,
    DISPLAY_INVERT,
    COLOR_DISPLAY_INVERT,
};

typedef bool (*OLED_INIT)(pegoist chip);
typedef int  (*OLED_CONF)(pegoist chip, enum OLED_MODE mode);
typedef int (*OLED_REFRESH)(pegoist chip);
typedef bool (*OLED_POWER)(pegoist chip, bool poweron);
typedef int (*OLED_CLEAR)(pegoist chip);

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
int ssd1306_clear(pegoist chip);
bool ssd1306_init(pegoist chip);
int ssd1306_conf(pegoist chip, enum OLED_MODE mode);
int ssd1306_refresh(pegoist chip);
bool ssd1306_power(pegoist chip, bool poweron);

/* Basic functions */
void oled_show_char(u8 x, u8 y, u8 ch, u8 size);
void oled_show_string(u8 x, u8 y, u8 *ch, u8 size);
void oled_show_num(u8 x, u8 y, u32 num, u8 len, u8 size);
void oled_draw_point(u8 x, u8 y);
void oled_draw_line(void);

#endif /* __OLED_LIB_H__ */