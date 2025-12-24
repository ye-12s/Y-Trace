#ifndef __SAMPLE_H__
#define __SAMPLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "drivers/drv_soft_iic.h"

int mag_sample(struct _soft_i2c_bus *bus);
int imu_sample(struct _soft_i2c_bus *bus);
int gnss_sample_init(void);

#ifdef __cplusplus
}
#endif

#endif