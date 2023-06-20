#ifndef __SSD1306_H__
#define __SSD1306_H__
#include <linux/i2c.h>


void ssd1306_init(void);
void ssd1306_display_on(void);
void ssd1306_display_off(void);
void ssd1306_refresh_gram(void);
void set_ssd1306_i2c_client(struct i2c_client*clinet);
void ssd1306_display_string(uint8_t chXpos, uint8_t chYpos, const uint8_t* pchString,
                            uint8_t chSize, uint8_t chMode);
void ssd1306_draw_rectangle(uint8_t chXpos1, uint8_t chYpos1, uint8_t xLen,
                            uint8_t yLen, uint8_t chDot);
void ssd1306_fill_screen(uint8_t chXpos1, uint8_t chYpos1, uint8_t xLen,
                         uint8_t yLen, uint8_t chDot);                            
void ssd1306_clear_screen(uint8_t chFill);
void ssd1306_draw_image(uint8_t *iamge);

#endif
