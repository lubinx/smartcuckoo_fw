#include <endian.h>
#include <stdlib.h>
#include <gpio.h>
#include <timer.h>

#include "irda.h"

// #define MOD_FREQ                        (38000U)
// #define MOD_PLUSE                       (1000000U / MOD_FREQ + 1U)

enum IrDA_frame_t
{
    IRDA_NONE,

    IRDA_SYNC_DA,
    IRDA_SYNC_END_DA,
    IRDA_SYNC_REPEAT,
    IRDA_SYNC_END,

    IRDA_LOGIC_1,
    IRDA_LOGIC_0,
};

#define MOD_SYNC_DA                     (9000U + 4500U)
#define MOD_SYNC_REPEAT                 (98000)
#define MOD_SYNC_END_DA                 (41000)
#define MOD_SYNC_END                    (11500)

#define MOD_LOGIC_1                     (2250U)
#define MOD_LOGIC_0                     (1125U)

#define MOD_ERR(MOD)                    ((MOD) * 5U / 100U)

static inline long unsigned int absl(long int val)
{
    return ((unsigned)(0 < val ? val : -val));
}

static enum IrDA_frame_t MOD_get_frame(uint32_t diff)
{
    if (MOD_ERR(MOD_LOGIC_1) > absl((int)(MOD_LOGIC_1 - diff)))
        return IRDA_LOGIC_1;
    if (MOD_ERR(MOD_LOGIC_0) > absl((int)(MOD_LOGIC_0 - diff)))
        return IRDA_LOGIC_0;

    if (MOD_ERR(MOD_SYNC_DA) > abs((int)(MOD_SYNC_DA - diff)))
        return IRDA_SYNC_DA;
    if (MOD_ERR(MOD_SYNC_END_DA) > abs((int)(MOD_SYNC_END_DA - diff)))
        return IRDA_SYNC_END_DA;

    if (MOD_ERR(MOD_SYNC_REPEAT) > absl((int)(MOD_SYNC_REPEAT - diff)))
        return IRDA_SYNC_REPEAT;

    if (MOD_ERR(MOD_SYNC_END) > absl((int)(MOD_SYNC_END - diff)))
        return IRDA_SYNC_END;

    return IRDA_NONE;
}

static void IrDA_gpio_callback(uint32_t pins, struct IrDA_attr_t *attr)
{
    ARG_UNUSED(pins);

    uint32_t tick = TIMER_tick(TIMER_NB_0);
    uint32_t diff = tick - attr->tick;
    attr->tick = tick;

    switch (MOD_get_frame(diff))
    {
    case IRDA_NONE:
        attr->idx = 0;
        break;

    case IRDA_SYNC_DA:
        attr->da.value = 0;
        attr->idx = 1;
        break;
    case IRDA_SYNC_END_DA:
        attr->idx = -1;
        break;

    case IRDA_SYNC_REPEAT:
        if (-1 == attr->idx || -2 == attr->idx)
            attr->idx = -2;
        break;
    case IRDA_SYNC_END:
        if (-2 == attr->idx)
            goto da_callback;
        break;

    case IRDA_LOGIC_1:
        if (0 < attr->idx)
        {
            attr->da.value = (attr->da.value << 1) | 1;
            if (32 == attr->idx ++)
                goto da_rbit_callback;
        }
        break;
    case IRDA_LOGIC_0:
        if (0 < attr->idx)
        {
            attr->da.value = (attr->da.value << 1) | 0;
            if (32 == attr->idx ++)
                goto da_rbit_callback;
        }
        break;
    }

    if (false)
    {
    da_rbit_callback:
        attr->da.value = __RBIT(attr->da.value);
    da_callback:
        attr->callback((struct IrDA_t *)&attr->da, 0 > attr->idx, attr->arg);
    }
}

void IrDA_init(struct IrDA_attr_t *attr, uint32_t PIN, IrDA_callback_t callback, void *arg)
{
    attr->idx = 0;

    attr->callback = callback;
    attr->arg = arg;
    attr->dev = TIMER0;

    TIMER_free_running_configure(TIMER_NB_0, 1000000);
    TIMER_start(TIMER_NB_0);

    GPIO_intr_enable(PIN, TRIG_BY_FALLING_EDGE, (void *)IrDA_gpio_callback, attr);
}
