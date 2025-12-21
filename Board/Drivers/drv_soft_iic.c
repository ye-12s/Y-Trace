#include "drv_soft_iic.h"
#include "drv_pin.h"
#include "rtthread.h"
#include "rthw.h"
#include "ulog.h"


#define SET_SCL(ops, val) pin_write(ops->scl, val)
#define GET_SCL(ops)      pin_read(ops->scl)

#define SDA_L(ops)        pin_write(ops->sda, 0)
#define SDA_H(ops)        pin_write(ops->sda, 1)
#define SCL_L(ops)        pin_write(ops->scl, 0)
// #define SCL_H(ops)     io_write(ops->scl, 1)
#define GET_SDA(ops)      pin_read(ops->sda)
#define SET_SDA(ops, val) pin_write(ops->sda, val)

#define i2c_delay(ops)    rt_hw_us_delay((ops->delay + 1) >> 1)
#define i2c_delay2(ops)   rt_hw_us_delay(ops->delay)

static int32_t SCL_H(struct _soft_i2c_bus *bus)
{
    uint32_t start;

    SET_SCL(bus, 1);

    start = rt_tick_get();
    while (!GET_SCL(bus))
    {
        if ((rt_tick_get() - start) > bus->timeout)
        {
            return -5;
        }
        i2c_delay(bus);
    }
#ifdef I2C_BITOPS_DEBUG
    if (rt_tick_get() != start)
    {
        LOG_D("wait %ld tick for SCL line to go high",
              rt_tick_get() - start);
    }
#endif

    i2c_delay(bus);

    return 0;
}

int drv_soft_i2c_init(struct _soft_i2c_bus *bus, pin_t scl, pin_t sda, int32_t speed, uint32_t timeout)
{
    if (!bus)
    {
        return -1;
    }
    bus->scl     = scl;
    bus->sda     = sda;
    bus->delay   = 1000 / speed;
    bus->timeout = timeout;
    pin_write(scl, 1);
    pin_write(sda, 1);
    pin_init(scl, PIN_MODE_OOD, PIN_PULL_NONE);
    pin_init(sda, PIN_MODE_OOD, PIN_PULL_NONE);
    bus->lock = rt_mutex_create("i2c_lock", RT_IPC_FLAG_FIFO);
    return 0;
}

int drv_soft_i2c_deinit(struct _soft_i2c_bus *bus)
{
    if (!bus)
    {
        return -1;
    }

    pin_write(bus->scl, 1);
    pin_write(bus->sda, 1);
    rt_mutex_delete(bus->lock);
    bus->lock = RT_NULL;
    return 0;
}

static void i2c_restart(struct _soft_i2c_bus *bus)
{
    SDA_H(bus);
    SCL_H(bus);
    i2c_delay(bus);
    SDA_L(bus);
    i2c_delay(bus);
    SCL_L(bus);
}

static void i2c_start(struct _soft_i2c_bus *bus)
{
#ifdef RT_I2C_BITOPS_DEBUG
    if (ops->get_scl && !GET_SCL(bus))
    {
        LOG_E("I2C bus error, SCL line low");
    }
    if (ops->get_sda && !GET_SDA(bus))
    {
        LOG_E("I2C bus error, SDA line low");
    }
#endif
    SDA_L(bus);
    i2c_delay(bus);
    SCL_L(bus);
}
static void i2c_stop(struct _soft_i2c_bus *bus)
{
    SDA_L(bus);
    i2c_delay(bus);
    SCL_H(bus);
    i2c_delay(bus);
    SDA_H(bus);
    i2c_delay2(bus);
}

int8_t i2c_waitack(struct _soft_i2c_bus *bus)
{
    int8_t ack;

    SDA_H(bus);
    i2c_delay(bus);

    if (SCL_H(bus) < 0)
    {
        LOG_W("wait ack timeout");
        return -5;
    }

    ack = !GET_SDA(bus);
    LOG_D("%s", ack ? "ACK" : "NACK");
    SCL_L(bus);

    return ack;
}

int32_t i2c_writeb(struct _soft_i2c_bus *bus,
                   uint8_t               data)
{
    int32_t i;
    uint8_t bit;

    for (i = 7; i >= 0; i--)
    {
        SCL_L(bus);
        bit = (data >> i) & 1;
        SET_SDA(bus, bit);
        i2c_delay(bus);
        if (SCL_H(bus) < 0)
        {
            LOG_D("i2c_writeb: 0x%02x, "
                  "wait scl pin high timeout at bit %d",
                  data, i);
            return -5;
        }
    }
    SCL_L(bus);
    i2c_delay(bus);

    return i2c_waitack(bus);
}

int32_t i2c_readb(struct _soft_i2c_bus *bus)
{
    uint8_t i;
    uint8_t data = 0;

    SDA_H(bus);
    i2c_delay(bus);
    for (i = 0; i < 8; i++)
    {
        data <<= 1;

        if (SCL_H(bus) < 0)
        {
            LOG_D("i2c_readb: wait scl pin high "
                  "timeout at bit %d",
                  7 - i);
            return -5;
        }

        if (GET_SDA(bus))
        {
            data |= 1;
        }
        SCL_L(bus);
        i2c_delay2(bus);
    }

    return data;
}

size_t i2c_send_bytes(struct _soft_i2c_bus *bus,
                      struct i2c_msg       *msg)
{
    int32_t        ret;
    size_t         bytes       = 0;
    const uint8_t *ptr         = msg->buf;
    int32_t        count       = msg->len;
    uint16_t       ignore_nack = msg->flags & I2C_FLAG_IGNORE_NACK;

    while (count > 0)
    {
        ret = i2c_writeb(bus, *ptr);

        if ((ret > 0) || (ignore_nack && (ret == 0)))
        {
            count--;
            ptr++;
            bytes++;
        }
        else if (ret == 0)
        {
            LOG_D("send bytes: NACK.");
            return 0;
        }
        else
        {
            LOG_E("send bytes: error %d", ret);
            return ret;
        }
    }

    return bytes;
}
int8_t i2c_send_ack_or_nack(struct _soft_i2c_bus *bus,
                            int                   ack)
{

    if (ack)
    {
        SET_SDA(bus, 0);
    }
    i2c_delay(bus);
    if (SCL_H(bus) < 0)
    {
        LOG_E("ACK or NACK timeout.");
        return -5;
    }
    SCL_L(bus);

    return 0;
}

size_t i2c_recv_bytes(struct _soft_i2c_bus *bus,
                      struct i2c_msg       *msg)
{
    int32_t        val;
    int32_t        bytes = 0; /* actual bytes */
    uint8_t       *ptr   = msg->buf;
    int32_t        count = msg->len;
    const uint32_t flags = msg->flags;

    while (count > 0)
    {
        val = i2c_readb(bus);
        if (val >= 0)
        {
            *ptr = val;
            bytes++;
        }
        else
        {
            break;
        }
        ptr++;
        count--;
        LOG_D("recieve bytes: 0x%02x, %s",
              val, (flags & I2C_FLAG_NO_READ_ACK) ? "(No ACK/NACK)" : (count ? "ACK" : "NACK"));

        if (!(flags & I2C_FLAG_NO_READ_ACK))
        {
            val = i2c_send_ack_or_nack(bus, count);
            if (val < 0)
            {
                return val;
            }
        }
    }
    return bytes;
}
int32_t i2c_send_address(struct _soft_i2c_bus *bus,
                         uint8_t               addr,
                         int32_t               retries)
{
    int32_t i;
    int8_t  ret = 0;

    for (i = 0; i <= retries; i++)
    {
        ret = i2c_writeb(bus, addr);
        if (ret == 1 || i == retries)
        {
            break;
        }
        LOG_D("send stop condition");
        i2c_stop(bus);
        i2c_delay2(bus);
        LOG_D("send start condition");
        i2c_start(bus);
    }

    return ret;
}
int8_t i2c_bit_send_address(struct _soft_i2c_bus *bus,
                            struct i2c_msg       *msg)
{
    uint16_t flags       = msg->flags;
    uint16_t ignore_nack = msg->flags & I2C_FLAG_IGNORE_NACK;

    uint8_t addr1, addr2;
    int32_t retries;
    int8_t  ret;

    retries = ignore_nack ? 0 : 3;

    if (flags & I2C_FLAG_ADDR_10BIT)
    {
        addr1 = 0xf0 | ((msg->addr >> 7) & 0x06);
        addr2 = msg->addr & 0xff;

        LOG_D("addr1: %d, addr2: %d", addr1, addr2);

        ret = i2c_send_address(bus, addr1, retries);
        if ((ret != 1) && !ignore_nack)
        {
            LOG_W("NACK: sending first addr");
            return -7;
        }

        ret = i2c_writeb(bus, addr2);
        if ((ret != 1) && !ignore_nack)
        {
            LOG_W("NACK: sending second addr");
            return -7;
        }
        if (flags & I2C_FLAG_RD)
        {
            LOG_D("send repeated start condition");
            i2c_restart(bus);
            addr1 |= 0x01;
            ret = i2c_send_address(bus, addr1, retries);
            if ((ret != 1) && !ignore_nack)
            {
                LOG_E("NACK: sending repeated addr");
                return -7;
            }
        }
    }
    else
    {
        /* 7-bit addr */
        addr1 = msg->addr << 1;
        if (flags & I2C_FLAG_RD)
        {
            addr1 |= 1;
        }
        ret = i2c_send_address(bus, addr1, retries);
        if ((ret != 1) && !ignore_nack)
        {
            return -7;
        }
    }
    return 0;
}

int i2c_bit_xfer(struct _soft_i2c_bus *bus,
                 struct i2c_msg        msgs[],
                 uint32_t              num)
{
    struct i2c_msg *msg;
    int32_t         ret;
    uint32_t        i;
    uint16_t        ignore_nack;

    for (i = 0; i < num; i++)
    {
        msg         = &msgs[i];
        ignore_nack = msg->flags & I2C_FLAG_IGNORE_NACK;
        if (!(msg->flags & I2C_FLAG_NO_START))
        {
            if (i)
            {
                i2c_restart(bus);
            }
            else
            {
                LOG_D("send start condition");
                i2c_start(bus);
            }
            ret = i2c_bit_send_address(bus, msg);
            if ((ret != 0) && !ignore_nack)
            {
                LOG_D("receive NACK from device addr 0x%02x msg %d",
                      msgs[i].addr, i);
                goto out;
            }
        }
        if (msg->flags & I2C_FLAG_RD)
        {
            ret = i2c_recv_bytes(bus, msg);
            if (ret >= 1)
            {
                LOG_D("read %d byte%s", ret, ret == 1 ? "" : "s");
            }
            if (ret < msg->len)
            {
                if (ret >= 0)
                {
                    ret = -7;
                }
                goto out;
            }
        }
        else
        {
            ret = i2c_send_bytes(bus, msg);
            if (ret >= 1)
            {
                LOG_D("write %d byte%s", ret, ret == 1 ? "" : "s");
            }
            if (ret < msg->len)
            {
                if (ret >= 0)
                {
                    ret = -1;
                }
                goto out;
            }
        }
    }
    ret = i;

out:
    if (!(msg->flags & I2C_FLAG_NO_STOP))
    {
        LOG_D("send stop condition");
        i2c_stop(bus);
    }
    return ret;
}

int drv_soft_i2c_transfer(struct _soft_i2c_bus *bus,
                          struct i2c_msg        msgs[],
                          uint32_t              num)
{
    int ret;
#ifdef I2C_DEBUG
    for (ret = 0; ret < num; ret++)
    {
        LOG_D("msgs[%d] %c, addr=0x%02x, len=%d", ret,
              (msgs[ret].flags & RT_I2C_RD) ? 'R' : 'W',
              msgs[ret].addr, msgs[ret].len);
    }
#endif
    ret = i2c_bit_xfer(bus, msgs, num);
    return ret;
}

int drv_soft_i2c_master_send(struct _soft_i2c_bus *bus,
                             uint16_t              addr,
                             uint16_t              flags,
                             const uint8_t        *buf,
                             size_t                count)
{
    struct i2c_msg msg;

    msg.addr  = addr;
    msg.buf   = (uint8_t *)buf;
    msg.len   = count;
    msg.flags = flags;
    rt_mutex_take(bus->lock, RT_WAITING_FOREVER);
    int ret = drv_soft_i2c_transfer(bus, &msg, 1);
    rt_mutex_release(bus->lock);
    return ret == 1 ? count : ret;
}

int drv_soft_i2c_master_recv(struct _soft_i2c_bus *bus,
                             uint16_t              addr,
                             uint16_t              flags,
                             uint8_t              *buf,
                             size_t                count)
{
    struct i2c_msg msg;

    msg.addr  = addr;
    msg.buf   = buf;
    msg.len   = count;
    msg.flags = flags | I2C_FLAG_RD;

    rt_mutex_take(bus->lock, RT_WAITING_FOREVER);
    int ret = drv_soft_i2c_transfer(bus, &msg, 1);
    rt_mutex_release(bus->lock);
    return ret == 1 ? count : ret;
}