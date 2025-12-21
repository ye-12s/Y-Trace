#include "Drivers/drv_lcd.h"
#include "rtthread.h"

#include "Drivers/drv_pin.h"
#include "Drivers/drv_soft_iic.h"

#include "sample/sample.h"
#include "port/littlefs_if.hpp"
#include "system/vfs/vfs.hpp"


#define LOG_TAG "app.init"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"

extern "C" {

    static littlefs::fs filesystem;
    static sys::VFS vfs;

    static struct _soft_i2c_bus sensor_i2c_bus;
    const pin_t PIN_SENSOR_I2C_SCL = GET_PIN(A, 8);
    const pin_t PIN_SENSOR_I2C_SDA = GET_PIN(C, 9);

    static int bsp_init(void)
    {
        drv_soft_i2c_init(&sensor_i2c_bus, PIN_SENSOR_I2C_SCL, PIN_SENSOR_I2C_SDA, 100, 1000);
        return 0;
    }
    INIT_DEVICE_EXPORT(bsp_init);

    static int sensor_init(void)
    {
        int ret = imu_sample(&sensor_i2c_bus);
        ASSERT(ret == RT_EOK);
        ret = mag_sample(&sensor_i2c_bus);
        ASSERT(ret == RT_EOK);
        return 0;
    }
// INIT_COMPONENT_EXPORT(sensor_init);

    static int lcd_init(void)
    {
        drv_lcd_init();
        LOG_I("LCD initialized.");
        return 0;
    }
    INIT_COMPONENT_EXPORT(lcd_init);

    static int fs_init(void)
    {
        filesystem.init();
        vfs.mount("/", &filesystem);

        return 0;
    }
    // INIT_COMPONENT_EXPORT(fs_init);

    static int app_init(void)
    {
        LOG_I("Application initialization completed.");
        return 0;
    }
    INIT_APP_EXPORT(app_init);


}
