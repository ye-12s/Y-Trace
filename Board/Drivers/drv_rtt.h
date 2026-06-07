#ifndef Y_TRACE_DRV_RTT_H
#define Y_TRACE_DRV_RTT_H

#include <rtthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef Y_TRACE_RTT_CB_SECTION
#define Y_TRACE_RTT_CB_SECTION __attribute__((section(".rtt"), used, aligned(4)))
#endif

void ytrace_rtt_init(void);
rt_size_t ytrace_rtt_write(unsigned int channel, const void *buffer, rt_size_t length);
void ytrace_rtt_write_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* Y_TRACE_DRV_RTT_H */
