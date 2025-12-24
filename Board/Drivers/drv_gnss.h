#ifndef __DRV_GNSS_H__
#define __DRV_GNSS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "rtthread.h"

typedef struct
{
    bool     fix_valid;      // RMC.valid && GGA.fix_quality > 0
    bool     fix_3d;         // GSA.fix_type == 3
    int      sats_used;      // GGA.satellites_tracked
    float    hdop;           // GGA.hdop
    float    latitude;       // degrees
    float    longitude;      // degrees
    float    altitude;       // meters
    float    speed_kph;      // km/h
    float    course_deg;     // degrees
    rt_tick_t   last_update;
} gps_state_t;

int drv_gnss_init(void);
int drv_gnss_get_line(char *line, int maxlen);
void drv_gnss_raw_show();

#ifdef __cplusplus
}
#endif

#endif