#include "drv_rtt.h"

#include <rthw.h>

#ifndef Y_TRACE_RTT_UP_BUFFER_SIZE
#define Y_TRACE_RTT_UP_BUFFER_SIZE 1024U
#endif

#define Y_TRACE_RTT_MODE_NO_BLOCK_SKIP 0U
#define Y_TRACE_RTT_MAX_UP_BUFFERS     1
#define Y_TRACE_RTT_MAX_DOWN_BUFFERS   0

typedef struct
{
    const char *name;
    char *buffer;
    unsigned int size;
    volatile unsigned int wr_off;
    volatile unsigned int rd_off;
    unsigned int flags;
} ytrace_rtt_up_buffer_t;

typedef struct
{
    const char *name;
    char *buffer;
    unsigned int size;
    volatile unsigned int wr_off;
    volatile unsigned int rd_off;
    unsigned int flags;
} ytrace_rtt_down_buffer_t;

typedef struct
{
    char id[16];
    int max_up_buffers;
    int max_down_buffers;
    ytrace_rtt_up_buffer_t up[Y_TRACE_RTT_MAX_UP_BUFFERS];
    ytrace_rtt_down_buffer_t down[1];
} ytrace_rtt_control_block_t;

static char ytrace_rtt_up_buffer[Y_TRACE_RTT_UP_BUFFER_SIZE];
static int ytrace_rtt_initialized = 0;

static ytrace_rtt_control_block_t ytrace_rtt_cb Y_TRACE_RTT_CB_SECTION;

void ytrace_rtt_init(void)
{
    static const char rtt_id[] = "SEGGER RTT";
    unsigned int i;

    if (ytrace_rtt_initialized) {
        return;
    }

    for (i = 0; i < sizeof(ytrace_rtt_cb.id); i++) {
        ytrace_rtt_cb.id[i] = '\0';
    }
    for (i = 0; i < sizeof(rtt_id) - 1U && i < sizeof(ytrace_rtt_cb.id); i++) {
        ytrace_rtt_cb.id[i] = rtt_id[i];
    }

    ytrace_rtt_cb.max_up_buffers   = Y_TRACE_RTT_MAX_UP_BUFFERS;
    ytrace_rtt_cb.max_down_buffers = Y_TRACE_RTT_MAX_DOWN_BUFFERS;

    ytrace_rtt_cb.up[0].name   = "Terminal";
    ytrace_rtt_cb.up[0].buffer = ytrace_rtt_up_buffer;
    ytrace_rtt_cb.up[0].size   = sizeof(ytrace_rtt_up_buffer);
    ytrace_rtt_cb.up[0].wr_off = 0;
    ytrace_rtt_cb.up[0].rd_off = 0;
    ytrace_rtt_cb.up[0].flags  = Y_TRACE_RTT_MODE_NO_BLOCK_SKIP;

    ytrace_rtt_cb.down[0].name   = RT_NULL;
    ytrace_rtt_cb.down[0].buffer = RT_NULL;
    ytrace_rtt_cb.down[0].size   = 0;
    ytrace_rtt_cb.down[0].wr_off = 0;
    ytrace_rtt_cb.down[0].rd_off = 0;
    ytrace_rtt_cb.down[0].flags  = 0;

    ytrace_rtt_initialized = 1;
}

static unsigned int ytrace_rtt_next_offset(unsigned int offset, unsigned int size)
{
    offset++;
    if (offset >= size) {
        offset = 0;
    }

    return offset;
}

rt_size_t ytrace_rtt_write(unsigned int channel, const void *buffer, rt_size_t length)
{
    const rt_uint8_t *src = (const rt_uint8_t *)buffer;
    ytrace_rtt_up_buffer_t *up;
    rt_base_t level;
    rt_size_t written = 0;

    ytrace_rtt_init();

    if (src == RT_NULL || channel >= (unsigned int)ytrace_rtt_cb.max_up_buffers) {
        return 0;
    }

    up = &ytrace_rtt_cb.up[channel];
    if (up->buffer == RT_NULL || up->size < 2U) {
        return 0;
    }

    level = rt_hw_interrupt_disable();
    while (written < length) {
        unsigned int wr_off = up->wr_off;
        unsigned int next   = ytrace_rtt_next_offset(wr_off, up->size);

        if (next == up->rd_off) {
            break;
        }

        up->buffer[wr_off] = (char)src[written++];
        __asm volatile("" ::: "memory");
        up->wr_off = next;
    }
    rt_hw_interrupt_enable(level);

    return written;
}

void ytrace_rtt_write_string(const char *str)
{
    const char *end;

    if (str == RT_NULL) {
        return;
    }

    end = str;
    while (*end != '\0') {
        end++;
    }

    (void)ytrace_rtt_write(0, str, (rt_size_t)(end - str));
}
