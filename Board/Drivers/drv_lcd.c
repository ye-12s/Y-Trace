//
// Created by An on 2025/12/14.
//

#include "drv_lcd.h"

#include <rtthread.h>
#include <stddef.h>

#include "at32f403a_407.h"
#include "drv_pin.h"

#define LCD_WIDTH 240
#define LCD_HEIGHT 320

#define LCD_CS_PIN GET_PIN(B, 12)
#define LCD_RST_PIN GET_PIN(B, 14)
#define LCD_DC_PIN GET_PIN(C, 6)
#define LCD_BL_PIN GET_PIN(B, 0)

#define LCD_SPIx SPI2
#define LCD_SPI_SDA GET_PIN(B, 15)
#define LCD_SPI_CLK GET_PIN(B, 13)

#define LCD_CS_ENABLE()  pin_write(LCD_CS_PIN, 0)
#define LCD_CS_DISABLE() pin_write(LCD_CS_PIN, 1)

#define LCD_CMD_ENABLE()  pin_write(LCD_DC_PIN, 1)
#define LCD_DATA_ENABLE() pin_write(LCD_DC_PIN, 0)

static void _lcd_hw_spi_init(void);
static void _lcd_reg_config(void);

static void write_byte(uint8_t byte) {
    spi_i2s_data_transmit(LCD_SPIx, byte);
    while ( spi_i2s_flag_get(LCD_SPIx, SPI_I2S_TDBE_FLAG) == RESET );
}

static void wr_cmd(uint16_t cmd) {
    LCD_CS_ENABLE();
    LCD_CMD_ENABLE();
    write_byte(cmd);
    LCD_CS_DISABLE();
}

static void wr_data(uint16_t data) {
    LCD_CS_ENABLE();
    LCD_DATA_ENABLE();
    write_byte(data >> 8);
    write_byte(data);
    LCD_CS_DISABLE();
}

void drv_lcd_set_course(size_t xStart, size_t yStart, size_t xEnd, size_t yEnd) {
    wr_cmd(0x2A); // Column addr set
    wr_data(xStart);
    wr_data(xEnd);

    wr_cmd(0x2B); // Row addr set
    wr_data(yStart);
    wr_data(yEnd);

    wr_cmd(0x2C); // Write to RAM
}

int drv_lcd_set_backlight(int level) {
    pin_write(LCD_BL_PIN, level);
    return 0;
}
int drv_lcd_clear(void) {

    return 0;
}
int drv_lcd_refresh(void) {

    return 0;
}

int drv_lcd_point(size_t x, size_t y, uint16_t color) {
    drv_lcd_set_course(x, y,x, y);
    wr_data(color);
    return 0;
}

int drv_lcd_full(size_t xStart, size_t yStart, size_t xEnd, size_t yEnd, uint16_t color) {
    drv_lcd_set_course(xStart, yStart, xEnd, yEnd);
    size_t total = (xEnd - xStart + 1) * (yEnd - yStart + 1);
    for (size_t i = 0; i < total; i++) {
        wr_data(color);
    }
    return 0;
}

int drv_lcd_init(void) {
    pin_init(LCD_CS_PIN, PIN_MODE_OPP,PIN_PULL_NONE);
    pin_init(LCD_BL_PIN, PIN_MODE_OPP,PIN_PULL_NONE);
    pin_init(LCD_DC_PIN, PIN_MODE_OPP,PIN_PULL_NONE);
    pin_init(LCD_RST_PIN, PIN_MODE_OPP,PIN_PULL_NONE);

    pin_write(LCD_CS_PIN, 1);
    pin_write(LCD_BL_PIN, 0);
    pin_write(LCD_DC_PIN, 0);
    pin_write(LCD_RST_PIN, 1);

    _lcd_hw_spi_init();

    //reset lcd
    pin_write(LCD_RST_PIN, 0);
    rt_thread_mdelay(10);
    pin_write(LCD_RST_PIN, 1);

    _lcd_reg_config();

    drv_lcd_set_backlight(1);
    rt_kprintf("drv_lcd_init\n");
    return 0;
}
// INIT_DEVICE_EXPORT(drv_lcd_init);

static void _lcd_reg_config(void) {
    wr_cmd(0x36);
    wr_data(0x00);

    wr_cmd(0x3A);
    wr_data(0x05);

    wr_cmd(0xB2);
    wr_data(0x0C);
    wr_data(0x0C);
    wr_data(0x00);
    wr_data(0x33);
    wr_data(0x33);

    wr_cmd(0xB7);
    wr_data(0x35);

    wr_cmd(0xBB);
    wr_data(0x19);

    wr_cmd(0xC0);
    wr_data(0x2C);

    wr_cmd(0xC2);
    wr_data(0x01);

    wr_cmd(0xC3);
    wr_data(0x12);

    wr_cmd(0xC4);
    wr_data(0x20);

    wr_cmd(0xC6);//刷新率
    wr_data(0x0F);

    wr_cmd(0xD0);
    wr_data(0xA4);
    wr_data(0xA1);

    wr_cmd(0xE0);
    wr_data(0xD0);
    wr_data(0x04);
    wr_data(0x0D);
    wr_data(0x11);
    wr_data(0x13);
    wr_data(0x2B);
    wr_data(0x3F);
    wr_data(0x54);
    wr_data(0x4C);
    wr_data(0x18);
    wr_data(0x0D);
    wr_data(0x0B);
    wr_data(0x1F);
    wr_data(0x23);

    wr_cmd(0xE1);
    wr_data(0xD0);
    wr_data(0x04);
    wr_data(0x0C);
    wr_data(0x11);
    wr_data(0x13);
    wr_data(0x2C);
    wr_data(0x3F);
    wr_data(0x44);
    wr_data(0x51);
    wr_data(0x2F);
    wr_data(0x1F);
    wr_data(0x1F);
    wr_data(0x20);
    wr_data(0x23);

    wr_cmd(0x21);

    wr_cmd(0x11);
    rt_thread_mdelay(10);
    wr_cmd(0x29);
}

static void _lcd_hw_spi_init(void) {
    gpio_init_type gpio_init_struct;
    spi_init_type spi_init_struct;

    gpio_default_para_init(&gpio_init_struct);
    spi_default_para_init(&spi_init_struct);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_13;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* configure the MOSI pin */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_15;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* configure param */
    spi_init_struct.transmission_mode = SPI_TRANSMIT_HALF_DUPLEX_TX;
    spi_init_struct.master_slave_mode = SPI_MODE_MASTER;
    spi_init_struct.frame_bit_num = SPI_FRAME_8BIT;
    spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB;
    spi_init_struct.mclk_freq_division = SPI_MCLK_DIV_8;
    spi_init_struct.clock_polarity = SPI_CLOCK_POLARITY_HIGH;
    spi_init_struct.clock_phase = SPI_CLOCK_PHASE_2EDGE;
    spi_init_struct.cs_mode_selection = SPI_CS_SOFTWARE_MODE;
    spi_init(SPI2, &spi_init_struct);

    spi_enable(SPI2, TRUE);
    write_byte(0x0000);
}
