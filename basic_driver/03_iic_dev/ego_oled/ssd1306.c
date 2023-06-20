#include "ssd1306.h"
#include "oled_font.h"

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

#define SSD1306_CMD    0
#define SSD1306_DAT    1

static uint8_t dispalyBuffer[128][8];
static struct i2c_client* ssd1306_client;

void set_ssd1306_i2c_client(struct i2c_client*clinet)
{
    ssd1306_client = clinet;
}

static void ssd1306_write_byte(uint8_t chData, uint8_t chCmd)
{
    uint8_t cmd = 0x00;

    if (chCmd) 
    {
        cmd = 0x40;
    }
    else {
        cmd = 0x00;
    }

    i2c_smbus_write_byte_data(ssd1306_client, cmd, chData);
}

void ssd1306_display_on(void)
{
    ssd1306_write_byte(0x8D, SSD1306_CMD);
    ssd1306_write_byte(0x14, SSD1306_CMD);
    ssd1306_write_byte(0xAF, SSD1306_CMD);
}

void ssd1306_display_off(void)
{
    ssd1306_write_byte(0x8D, SSD1306_CMD);
    ssd1306_write_byte(0x10, SSD1306_CMD);
    ssd1306_write_byte(0xAE, SSD1306_CMD);
}

void ssd1306_refresh_gram(void)
{
    uint8_t i, j;

    for (i = 0; i < 8; i++) 
    {
        ssd1306_write_byte(0xB0 + i, SSD1306_CMD);
        ssd1306_write_byte(0x02, SSD1306_CMD);
        ssd1306_write_byte(0x10, SSD1306_CMD);
        for (j = 0; j < 128; j++) {
            ssd1306_write_byte(dispalyBuffer[j][i], SSD1306_DAT);
        }
    }
}

void ssd1306_clear_screen(uint8_t chFill)
{
    memset(dispalyBuffer, chFill, sizeof(dispalyBuffer));
    ssd1306_refresh_gram();
}

void ssd1306_draw_point(uint8_t chXpos, uint8_t chYpos, uint8_t chPoint)
{
    uint8_t chPos, chBx, chTemp = 0;

    if (chXpos > 127 || chYpos > 63) 
    {
        printk("ssd1306_draw_point error\r\n");
        return;
    }
    chPos = 7 - chYpos / 8;
    chBx = chYpos % 8;
    chTemp = 1 << (7 - chBx);

    if (chPoint)
    {
        dispalyBuffer[chXpos][chPos] |= chTemp;
    }
    else 
    {  
        dispalyBuffer[chXpos][chPos] &= ~chTemp;
    }
}

void ssd1306_fill_screen(uint8_t chXpos1, uint8_t chYpos1, uint8_t xLen,
                         uint8_t yLen, uint8_t chDot)
{
    uint8_t chXpos, chYpos;

    uint8_t x1 = chXpos1;
    uint8_t x2 = chXpos1 + xLen;
    uint8_t y1 = chYpos1;
    uint8_t y2 = chYpos1 + yLen;

    for (chXpos = x1; chXpos <= x2; chXpos++) 
    {
        for (chYpos = y1; chYpos <= y2; chYpos++)
        {
           ssd1306_draw_point(chXpos, chYpos, chDot);
        }
    }

    ssd1306_refresh_gram();
}

void ssd1306_draw_rectangle(uint8_t chXpos1, uint8_t chYpos1, uint8_t xLen,
                            uint8_t yLen, uint8_t chDot)
{
    uint8_t chXpos, chYpos;
    uint8_t x1 = chXpos1;
    uint8_t x2 = chXpos1 + xLen;
    uint8_t y1 = chYpos1;
    uint8_t y2 = chYpos1 + yLen;

    for (chXpos = x1; chXpos <= x2; chXpos++) 
    {
        for (chYpos = y1; chYpos <= y2; chYpos++)
        {
            if (chXpos == x1 || chXpos == x2 || chYpos == y1 || chYpos == y2)
            {
                ssd1306_draw_point(chXpos, chYpos, chDot);
            }  
        }
    }

    ssd1306_refresh_gram();
}

void ssd1306_display_char(uint8_t chXpos, uint8_t chYpos, uint8_t chChr, uint8_t chSize, uint8_t chMode)
{
    uint8_t i, j;
    uint8_t chTemp, chYpos0 = chYpos;

    chChr = chChr - ' ';
    for (i = 0; i < chSize; i++) 
    {
        if (chSize == 16)
        {
            if (chMode)
            {
                chTemp = c_chFont1608[chChr][i];
            }
            else
            {
                chTemp = ~c_chFont1608[chChr][i];
            }
        }
        else
        {
            if (chMode)
            {
                chTemp = asc2_1206[chChr][i];
            }
            else
            {
                chTemp = ~asc2_1206[chChr][i];
            }
        }

        for (j = 0; j < 8; j++)
        {
            if (chTemp & 0x80)
            {
                ssd1306_draw_point(chXpos, chYpos, 1);
            }
            else 
            {
                ssd1306_draw_point(chXpos, chYpos, 0);
            }
            chTemp <<= 1;
            chYpos++;

            if ((chYpos - chYpos0) == chSize) 
            {
                chYpos = chYpos0;
                chXpos++;
                break;
            }
        }
    }
}

void ssd1306_display_string(uint8_t chXpos, uint8_t chYpos, const uint8_t* pchString, uint8_t chSize, uint8_t chMode)
{
    //printk("%s, ssd1306 str = %s\n", __func__, pchString);
    while (*pchString != '\0') 
    {
        if (chXpos > (SSD1306_WIDTH - chSize / 2)) 
        {
            chXpos = 0;
            chYpos += chSize;
            if (chYpos > (SSD1306_HEIGHT - chSize)) 
            {
                chYpos = chXpos = 0;
                ssd1306_clear_screen(0x00);
            }
        }

        ssd1306_display_char(chXpos, chYpos, *pchString, chSize, chMode);
        chXpos += chSize / 2;
        pchString++;
    }
}

void ssd1306_draw_image(uint8_t *iamge)
{
    memcpy(dispalyBuffer, iamge, 1024);
    ssd1306_refresh_gram();
}

void ssd1306_init(void)
{
    ssd1306_write_byte(0xAE, SSD1306_CMD);//--turn off oled panel
    ssd1306_write_byte(0x00, SSD1306_CMD);//---set low column address
    ssd1306_write_byte(0x10, SSD1306_CMD);//---set high column address
    ssd1306_write_byte(0x40, SSD1306_CMD);//--set start line address  Set Mapping RAM Display Start Line (0x00~0x3F)
    ssd1306_write_byte(0x81, SSD1306_CMD);//--set contrast control register
    ssd1306_write_byte(0xCF, SSD1306_CMD);// Set SEG Output Current Brightness
    ssd1306_write_byte(0xA1, SSD1306_CMD);//--Set SEG/Column Mapping
    ssd1306_write_byte(0xC0, SSD1306_CMD);//Set COM/Row Scan Direction
    ssd1306_write_byte(0xA6, SSD1306_CMD);//--set normal display
    ssd1306_write_byte(0xA8, SSD1306_CMD);//--set multiplex ratio(1 to 64)
    ssd1306_write_byte(0x3f, SSD1306_CMD);//--1/64 duty
    ssd1306_write_byte(0xD3, SSD1306_CMD);//-set display offset    Shift Mapping RAM Counter (0x00~0x3F)
    ssd1306_write_byte(0x00, SSD1306_CMD);//-not offset
    ssd1306_write_byte(0xd5, SSD1306_CMD);//--set display clock divide ratio/oscillator frequency
    ssd1306_write_byte(0x80, SSD1306_CMD);//--set divide ratio, Set Clock as 100 Frames/Sec
    ssd1306_write_byte(0xD9, SSD1306_CMD);//--set pre-charge period
    ssd1306_write_byte(0xF1, SSD1306_CMD);//Set Pre-Charge as 15 Clocks & Discharge as 1 Clock
    ssd1306_write_byte(0xDA, SSD1306_CMD);//--set com pins hardware configuration
    ssd1306_write_byte(0x12, SSD1306_CMD);
    ssd1306_write_byte(0xDB, SSD1306_CMD);//--set vcomh
    ssd1306_write_byte(0x40, SSD1306_CMD);//Set VCOM Deselect Level
    ssd1306_write_byte(0x20, SSD1306_CMD);//-Set Page Addressing Mode (0x00/0x01/0x02)
    ssd1306_write_byte(0x02, SSD1306_CMD);//
    ssd1306_write_byte(0x8D, SSD1306_CMD);//--set Charge Pump enable/disable
    ssd1306_write_byte(0x14, SSD1306_CMD);//--set(0x10) disable
    ssd1306_write_byte(0xA4, SSD1306_CMD);// Disable Entire Display On (0xa4/0xa5)
    ssd1306_write_byte(0xA6, SSD1306_CMD);// Disable Inverse Display On (0xa6/a7)
    ssd1306_write_byte(0xAF, SSD1306_CMD);//--turn on oled panel
}
