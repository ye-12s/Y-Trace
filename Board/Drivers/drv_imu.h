#ifndef __DRV_IMU_H__
#define __DRV_IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "drv_lis3mdltr.h"
#include "drv_lsm6dsm.h"
#include "drv_pin.h"
#include "drv_soft_iic.h"

#define DRV_IMU_FLAG_LSM6DSM_READY (1U << 0)
#define DRV_IMU_FLAG_LIS3MDL_READY (1U << 1)
#define DRV_IMU_FLAG_SAMPLE_VALID  (1U << 2)

typedef struct drv_imu_sample {
    lsm6dsm_accel_data_t accel;
    lsm6dsm_gyro_data_t gyro;
    lis3mdltr_mag_data_t mag;
    lsm6dsm_temp_data_t lsm6dsm_temp;
    lis3mdltr_temp_data_t lis3mdl_temp;
    uint8_t valid_flags;
} drv_imu_sample_t;

typedef struct drv_imu_status {
    uint8_t lsm6dsm_found;
    uint8_t lis3mdl_found;
    uint8_t lsm6dsm_addr;
    uint8_t lis3mdl_addr;
    uint8_t lsm6dsm_id;
    uint8_t lis3mdl_id;
    int8_t int1_level;
    int8_t int2_level;
    int last_error;
} drv_imu_status_t;

int drv_imu_bind(struct _soft_i2c_bus *bus, pin_t int1_pin, pin_t int2_pin);
int drv_imu_init(void);
int drv_imu_read(drv_imu_sample_t *sample);
int drv_imu_get_status(drv_imu_status_t *status);
int drv_imu_read_int_status(int8_t *int1_level, int8_t *int2_level);

#ifdef __cplusplus
}
#endif

#endif
