#include "Drivers/drv_gnss.h"
#include "rtthread.h"
#include "utils/minmea/minmea.h"


#define LOG_TAG "gnss.sample"
#define LOG_LVL LOG_LVL_DBG
#include "ulog.h"


gps_state_t g_gps_state = {0};


static void gps_parse_line(const char *line)
{
    switch (minmea_sentence_id(line, false))
    {

    case MINMEA_SENTENCE_GGA:
    {
        struct minmea_sentence_gga gga;
        if (minmea_parse_gga(&gga, line))
        {
            g_gps_state.sats_used = gga.satellites_tracked;
            g_gps_state.hdop = minmea_tofloat(&gga.hdop);
            g_gps_state.altitude = minmea_tofloat(&gga.altitude);

            g_gps_state.latitude =
                minmea_tocoord(&gga.latitude);
            g_gps_state.longitude =
                minmea_tocoord(&gga.longitude);

            g_gps_state.fix_valid = (gga.fix_quality > 0);
        }
        break;
    }

    case MINMEA_SENTENCE_RMC:
    {
        struct minmea_sentence_rmc rmc;
        if (minmea_parse_rmc(&rmc, line))
        {
            g_gps_state.fix_valid &= rmc.valid;
            g_gps_state.speed_kph =
                minmea_tofloat(&rmc.speed) * 1.852f;
            g_gps_state.course_deg =
                minmea_tofloat(&rmc.course);
        }
        break;
    }

    case MINMEA_SENTENCE_VTG:
    {
        struct minmea_sentence_vtg vtg;
        if (minmea_parse_vtg(&vtg, line))
        {
            g_gps_state.speed_kph =
                minmea_tofloat(&vtg.speed_kph);
        }
        break;
    }

    case MINMEA_SENTENCE_GSA:
    {
        struct minmea_sentence_gsa gsa;
        if (minmea_parse_gsa(&gsa, line))
        {
            g_gps_state.fix_3d = (gsa.fix_type == 3);
        }
        break;
    }

    default:
        break;
    }

    g_gps_state.last_update = rt_tick_get();
}


static void gps_parse_entry(void *arg)
{
    drv_gnss_init();

    const int maxlen = 256;
    char *line = rt_malloc(maxlen);
    ASSERT(line != NULL);

    while (1)
    {
        if (drv_gnss_get_line(line, maxlen))
        {
            LOG_D("gps line: (%d) %s", rt_strlen(line), line);
            gps_parse_line(line);


        }

        static int count = 0;
        if (count ++ > 100)
        {
            // LOG_I(" gps pos:<%.6f, %.6f>",
            //       g_gps_state.latitude,
            //       g_gps_state.longitude);
            LOG_I(" gps fix:%d 3D:%d sats:%d hdop:%.1f alt:%.1f spd:%.1f course:%.1f",
                  g_gps_state.fix_valid,
                  g_gps_state.fix_3d,
                  g_gps_state.sats_used,
                  g_gps_state.hdop,
                  g_gps_state.altitude,
                  g_gps_state.speed_kph,
                  g_gps_state.course_deg);
            count = 0;
        }
    }
}

int gnss_sample_init(void)
{
    rt_thread_t thread = rt_thread_create("gps_parse",
                                          gps_parse_entry,
                                          NULL,
                                          2048,
                                          RT_THREAD_PRIORITY_MAX / 3,
                                          10);
    ASSERT(thread != NULL);
    rt_thread_startup(thread);
    return 0;
}

