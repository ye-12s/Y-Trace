//
// Created by An on 2025/12/14.
//

#include "drv_lcd.h"

#include <stdbool.h>
#include <rtthread.h>
#include <stddef.h>
#include <stdint.h>

#include "at32f403a_407.h"
#include "drv_pin.h"

#define LCD_WIDTH  240U
#define LCD_HEIGHT 320U

#ifndef LCD_CS_PIN
/* SPI2 SCK is PB13. Use PB12 for CS, or define LCD_CS_PIN as -1 if LCD CS is tied low. */
#define LCD_CS_PIN GET_PIN(B, 12)
#endif

#define LCD_RST_PIN GET_PIN(B, 14)
#define LCD_DC_PIN  GET_PIN(C, 6)
#define LCD_BL_PIN  GET_PIN(B, 0)

#define LCD_SPIx    SPI2
#define LCD_SPI_SDA GET_PIN(B, 15)
#define LCD_SPI_CLK GET_PIN(B, 13)

#ifndef LCD_SPI_MCLK_DIVISION
#define LCD_SPI_MCLK_DIVISION SPI_MCLK_DIV_2
#endif

#define LCD_DMAx         DMA1
#define LCD_DMA_CHANNEL  DMA1_CHANNEL5
#define LCD_DMA_FLEX_CH  FLEX_CHANNEL5
#define LCD_DMA_TX_REQ   DMA_FLEXIBLE_SPI2_TX
#define LCD_DMA_DONE     DMA1_FDT5_FLAG
#define LCD_DMA_ERR      DMA1_DTERR5_FLAG
#define LCD_DMA_CLR      DMA1_GL5_FLAG
#define LCD_DMA_MAX_SIZE UINT16_MAX
#define LCD_DMA_INT      (DMA_FDT_INT | DMA_DTERR_INT)

#define LCD_COLOR_BLACK  0x0000U

#define LCD_CMD_MODE()   pin_write(LCD_DC_PIN, 0)
#define LCD_DATA_MODE()  pin_write(LCD_DC_PIN, 1)

static void _lcd_hw_spi_init(void);
static void _lcd_reg_config(void);
static void _lcd_reset(void);
static bool _lcd_has_cs(void);
static void _lcd_select(void);
static void _lcd_deselect(void);
static void _lcd_write_cmd(uint8_t cmd);
static void _lcd_write_data8(uint8_t data);
static void _lcd_write_data(const uint8_t *data, size_t len);
static void _lcd_write_color_repeat(uint16_t color, size_t count);
static int _lcd_spi_write_dma(const uint8_t *data, size_t len);
static int _lcd_spi_write_dma_start(const uint8_t *data, size_t len, bool irq_enable);
static void _lcd_spi_write_dma_stop(void);

typedef struct {
    volatile bool active;
    drv_lcd_write_done_cb_t done_cb;
    void *user_data;
} lcd_dma_async_state_t;

static lcd_dma_async_state_t lcd_dma_async_state;

static bool _lcd_has_cs(void)
{
    return LCD_CS_PIN >= 0 && LCD_CS_PIN != LCD_SPI_CLK;
}

static void _lcd_select(void)
{
    if (_lcd_has_cs()) {
        pin_write(LCD_CS_PIN, 0);
    }
}

static void _lcd_spi_wait_idle(void)
{
    while (spi_i2s_flag_get(LCD_SPIx, SPI_I2S_TDBE_FLAG) == RESET) {
    }
    while (spi_i2s_flag_get(LCD_SPIx, SPI_I2S_BF_FLAG) == SET) {
    }
}

static void _lcd_deselect(void)
{
    _lcd_spi_wait_idle();
    if (_lcd_has_cs()) {
        pin_write(LCD_CS_PIN, 1);
    }
}

static void _lcd_spi_write_byte(uint8_t byte)
{
    while (spi_i2s_flag_get(LCD_SPIx, SPI_I2S_TDBE_FLAG) == RESET) {
    }
    spi_i2s_data_transmit(LCD_SPIx, byte);
}

static void _lcd_spi_write(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        _lcd_spi_write_byte(data[i]);
    }
}

static int _lcd_spi_write_dma_start(const uint8_t *data, size_t len, bool irq_enable)
{
    if (data == RT_NULL || len == 0) {
        return -RT_EINVAL;
    }
    if (len > LCD_DMA_MAX_SIZE) {
        return -RT_EINVAL;
    }

    dma_init_type dma_init_struct;

    dma_channel_enable(LCD_DMA_CHANNEL, FALSE);
    dma_interrupt_enable(LCD_DMA_CHANNEL, LCD_DMA_INT, FALSE);
    dma_flag_clear(LCD_DMA_CLR);
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.peripheral_base_addr  = (uint32_t)&LCD_SPIx->dt;
    dma_init_struct.memory_base_addr      = (uint32_t)data;
    dma_init_struct.direction             = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.buffer_size           = (uint16_t)len;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable     = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.loop_mode_enable      = FALSE;
    dma_init_struct.priority              = DMA_PRIORITY_VERY_HIGH;
    dma_init(LCD_DMA_CHANNEL, &dma_init_struct);

    if (irq_enable) {
        dma_interrupt_enable(LCD_DMA_CHANNEL, LCD_DMA_INT, TRUE);
    }

    spi_i2s_dma_transmitter_enable(LCD_SPIx, TRUE);
    dma_channel_enable(LCD_DMA_CHANNEL, TRUE);

    return 0;
}

static void _lcd_spi_write_dma_stop(void)
{
    dma_channel_enable(LCD_DMA_CHANNEL, FALSE);
    dma_interrupt_enable(LCD_DMA_CHANNEL, LCD_DMA_INT, FALSE);
    spi_i2s_dma_transmitter_enable(LCD_SPIx, FALSE);
    dma_flag_clear(LCD_DMA_CLR);
}

static int _lcd_spi_write_dma(const uint8_t *data, size_t len)
{
    if (data == RT_NULL || len == 0) {
        return -RT_EINVAL;
    }

    while (lcd_dma_async_state.active) {
        rt_thread_mdelay(1);
    }

    while (len > 0) {
        size_t chunk = len > LCD_DMA_MAX_SIZE ? LCD_DMA_MAX_SIZE : len;
        int result   = _lcd_spi_write_dma_start(data, chunk, false);
        if (result != 0) {
            return result;
        }

        while (dma_flag_get(LCD_DMA_DONE) == RESET) {
            if (dma_flag_get(LCD_DMA_ERR) == SET) {
                _lcd_spi_write_dma_stop();
                return -RT_ERROR;
            }
        }

        _lcd_spi_write_dma_stop();
        _lcd_spi_wait_idle();

        data += chunk;
        len -= chunk;
    }

    return 0;
}

static void _lcd_write_cmd(uint8_t cmd)
{
    _lcd_select();
    LCD_CMD_MODE();
    _lcd_spi_write_byte(cmd);
    _lcd_deselect();
}

static void _lcd_write_data8(uint8_t data)
{
    _lcd_select();
    LCD_DATA_MODE();
    _lcd_spi_write_byte(data);
    _lcd_deselect();
}

static void _lcd_write_data(const uint8_t *data, size_t len)
{
    if (data == RT_NULL || len == 0) {
        return;
    }

    _lcd_select();
    LCD_DATA_MODE();
    _lcd_spi_write(data, len);
    _lcd_deselect();
}

static void _lcd_write_color_repeat(uint16_t color, size_t count)
{
    uint8_t pixels[128];
    size_t pixels_per_chunk = sizeof(pixels) / 2U;

    for (size_t i = 0; i < pixels_per_chunk; i++) {
        pixels[i * 2U]      = (uint8_t)(color >> 8);
        pixels[i * 2U + 1U] = (uint8_t)color;
    }

    _lcd_select();
    LCD_DATA_MODE();
    while (count > 0) {
        size_t chunk = count > pixels_per_chunk ? pixels_per_chunk : count;
        if (_lcd_spi_write_dma(pixels, chunk * 2U) != 0) {
            break;
        }
        count -= chunk;
    }
    _lcd_deselect();
}

int drv_lcd_set_window(size_t xStart, size_t yStart, size_t xEnd, size_t yEnd)
{
    if (xStart >= LCD_WIDTH || yStart >= LCD_HEIGHT || xStart > xEnd || yStart > yEnd) {
        return -RT_EINVAL;
    }

    if (xEnd >= LCD_WIDTH) {
        xEnd = LCD_WIDTH - 1U;
    }
    if (yEnd >= LCD_HEIGHT) {
        yEnd = LCD_HEIGHT - 1U;
    }

    uint8_t column[] = {
        (uint8_t)(xStart >> 8),
        (uint8_t)xStart,
        (uint8_t)(xEnd >> 8),
        (uint8_t)xEnd,
    };
    uint8_t row[] = {
        (uint8_t)(yStart >> 8),
        (uint8_t)yStart,
        (uint8_t)(yEnd >> 8),
        (uint8_t)yEnd,
    };

    _lcd_write_cmd(0x2A);
    _lcd_write_data(column, sizeof(column));

    _lcd_write_cmd(0x2B);
    _lcd_write_data(row, sizeof(row));

    _lcd_write_cmd(0x2C);
    return 0;
}

size_t drv_lcd_get_width(void)
{
    return LCD_WIDTH;
}

size_t drv_lcd_get_height(void)
{
    return LCD_HEIGHT;
}

int drv_lcd_set_backlight(int level)
{
    pin_write(LCD_BL_PIN, level ? 1 : 0);
    return 0;
}

int drv_lcd_clear(void)
{
    return drv_lcd_full(0, 0, LCD_WIDTH - 1U, LCD_HEIGHT - 1U, LCD_COLOR_BLACK);
}

int drv_lcd_refresh(void)
{

    return 0;
}

int drv_lcd_point(size_t x, size_t y, uint16_t color)
{
    if (drv_lcd_set_window(x, y, x, y) != 0) {
        return -RT_EINVAL;
    }
    _lcd_write_color_repeat(color, 1);
    return 0;
}

int drv_lcd_full(size_t xStart, size_t yStart, size_t xEnd, size_t yEnd, uint16_t color)
{
    if (drv_lcd_set_window(xStart, yStart, xEnd, yEnd) != 0) {
        return -RT_EINVAL;
    }

    if (xEnd >= LCD_WIDTH) {
        xEnd = LCD_WIDTH - 1U;
    }
    if (yEnd >= LCD_HEIGHT) {
        yEnd = LCD_HEIGHT - 1U;
    }

    size_t total = (xEnd - xStart + 1U) * (yEnd - yStart + 1U);
    _lcd_write_color_repeat(color, total);
    return 0;
}

int drv_lcd_write_pixels(const uint16_t *pixels, size_t count)
{
    if (pixels == RT_NULL || count == 0) {
        return -RT_EINVAL;
    }

    _lcd_select();
    LCD_DATA_MODE();
    for (size_t i = 0; i < count; i++) {
        uint8_t pixel[] = {
            (uint8_t)(pixels[i] >> 8),
            (uint8_t)pixels[i],
        };
        _lcd_spi_write(pixel, sizeof(pixel));
    }
    _lcd_deselect();
    return 0;
}

int drv_lcd_write_bytes(const uint8_t *data, size_t len)
{
    if (data == RT_NULL || len == 0) {
        return -RT_EINVAL;
    }

    _lcd_select();
    LCD_DATA_MODE();
    int result = _lcd_spi_write_dma(data, len);
    _lcd_deselect();
    return result;
}

int drv_lcd_write_bytes_async(const uint8_t *data, size_t len, drv_lcd_write_done_cb_t done_cb, void *user_data)
{
    if (data == RT_NULL || len == 0 || done_cb == RT_NULL) {
        return -RT_EINVAL;
    }
    if (len > LCD_DMA_MAX_SIZE) {
        return -RT_EINVAL;
    }
    if (lcd_dma_async_state.active) {
        return -RT_EBUSY;
    }

    _lcd_select();
    LCD_DATA_MODE();

    lcd_dma_async_state.done_cb   = done_cb;
    lcd_dma_async_state.user_data = user_data;
    lcd_dma_async_state.active    = true;

    int result = _lcd_spi_write_dma_start(data, len, true);
    if (result != 0) {
        lcd_dma_async_state.active    = false;
        lcd_dma_async_state.done_cb   = RT_NULL;
        lcd_dma_async_state.user_data = RT_NULL;
        _lcd_deselect();
        return result;
    }

    return 0;
}

int drv_lcd_init(void)
{
    if (_lcd_has_cs()) {
        pin_init(LCD_CS_PIN, PIN_MODE_OPP, PIN_PULL_NONE);
        pin_write(LCD_CS_PIN, 1);
    }
    pin_init(LCD_BL_PIN, PIN_MODE_OPP, PIN_PULL_NONE);
    pin_init(LCD_DC_PIN, PIN_MODE_OPP, PIN_PULL_NONE);
    pin_init(LCD_RST_PIN, PIN_MODE_OPP, PIN_PULL_NONE);

    pin_write(LCD_BL_PIN, 0);
    LCD_CMD_MODE();

    _lcd_hw_spi_init();
    _lcd_reset();
    _lcd_reg_config();

    drv_lcd_set_backlight(1);
    drv_lcd_clear();
    rt_kprintf("drv_lcd_init\n");
    return 0;
}
// INIT_DEVICE_EXPORT(drv_lcd_init);

static void _lcd_reset(void)
{
    pin_write(LCD_RST_PIN, 1);
    rt_thread_mdelay(120);
    pin_write(LCD_RST_PIN, 0);
    rt_thread_mdelay(120);
    pin_write(LCD_RST_PIN, 1);
    rt_thread_mdelay(120);
}

static void _lcd_reg_config(void)
{
    static const uint8_t porch[]          = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    static const uint8_t power[]          = {0xA4, 0xA1};
    static const uint8_t gamma_positive[] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
    static const uint8_t gamma_negative[] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};

    _lcd_write_cmd(0x11);
    rt_thread_mdelay(120);

    _lcd_write_cmd(0x36);
    _lcd_write_data8(0x00);

    _lcd_write_cmd(0x3A);
    _lcd_write_data8(0x05);

    _lcd_write_cmd(0xB2);
    _lcd_write_data(porch, sizeof(porch));

    _lcd_write_cmd(0xB7);
    _lcd_write_data8(0x35);

    _lcd_write_cmd(0xBB);
    _lcd_write_data8(0x19);

    _lcd_write_cmd(0xC0);
    _lcd_write_data8(0x2C);

    _lcd_write_cmd(0xC2);
    _lcd_write_data8(0x01);

    _lcd_write_cmd(0xC3);
    _lcd_write_data8(0x12);

    _lcd_write_cmd(0xC4);
    _lcd_write_data8(0x20);

    _lcd_write_cmd(0xC6);
    _lcd_write_data8(0x0F);

    _lcd_write_cmd(0xD0);
    _lcd_write_data(power, sizeof(power));

    _lcd_write_cmd(0xE0);
    _lcd_write_data(gamma_positive, sizeof(gamma_positive));

    _lcd_write_cmd(0xE1);
    _lcd_write_data(gamma_negative, sizeof(gamma_negative));

    _lcd_write_cmd(0x29);
    rt_thread_mdelay(20);
}

static void _lcd_hw_spi_init(void)
{
    gpio_init_type gpio_init_struct;
    spi_init_type spi_init_struct;

    gpio_default_para_init(&gpio_init_struct);
    spi_default_para_init(&spi_init_struct);

    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);
    dma_flexible_config(LCD_DMAx, LCD_DMA_FLEX_CH, LCD_DMA_TX_REQ);
    nvic_irq_enable(DMA1_Channel5_IRQn, 1, 0);

    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins           = GPIO_PINS_13;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* configure the MOSI pin */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_STRONGER;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins           = GPIO_PINS_15;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* configure param */
    spi_init_struct.transmission_mode      = SPI_TRANSMIT_HALF_DUPLEX_TX;
    spi_init_struct.master_slave_mode      = SPI_MODE_MASTER;
    spi_init_struct.frame_bit_num          = SPI_FRAME_8BIT;
    spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB;
    spi_init_struct.mclk_freq_division     = LCD_SPI_MCLK_DIVISION;
    spi_init_struct.clock_polarity         = SPI_CLOCK_POLARITY_HIGH;
    spi_init_struct.clock_phase            = SPI_CLOCK_PHASE_2EDGE;
    spi_init_struct.cs_mode_selection      = SPI_CS_SOFTWARE_MODE;
    spi_init(SPI2, &spi_init_struct);
    spi_software_cs_internal_level_set(SPI2, SPI_SWCS_INTERNAL_LEVEL_HIGHT);

    spi_enable(SPI2, TRUE);
    _lcd_spi_write_byte(0x00);
    _lcd_spi_wait_idle();
}

void DMA1_Channel5_IRQHandler(void)
{
    rt_interrupt_enter();

    if (lcd_dma_async_state.active && (dma_interrupt_flag_get(LCD_DMA_DONE) == SET || dma_interrupt_flag_get(LCD_DMA_ERR) == SET)) {
        int result                      = dma_interrupt_flag_get(LCD_DMA_ERR) == SET ? -RT_ERROR : 0;
        drv_lcd_write_done_cb_t done_cb = lcd_dma_async_state.done_cb;
        void *user_data                 = lcd_dma_async_state.user_data;

        _lcd_spi_write_dma_stop();
        _lcd_spi_wait_idle();
        if (_lcd_has_cs()) {
            pin_write(LCD_CS_PIN, 1);
        }

        lcd_dma_async_state.done_cb   = RT_NULL;
        lcd_dma_async_state.user_data = RT_NULL;
        lcd_dma_async_state.active    = false;

        if (done_cb != RT_NULL) {
            done_cb(user_data, result);
        }
    } else {
        dma_flag_clear(LCD_DMA_CLR);
    }

    rt_interrupt_leave();
}
