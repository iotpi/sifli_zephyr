#define DT_DRV_COMPAT sifli_lptim

#include <zephyr/kernel.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/device.h>
#include <zephyr/irq.h>
#include <zephyr/sys_clock.h>
#include <zephyr/spinlock.h>
#include "bf0_hal_lptim.h"
#include "register.h"

#define LPTIM_CLOCK_FREQ 32768  /* LPTIM clock frequency in Hz */
#define LPTIM_PRESCALER 0       /* No prescaler */
#define LPTIM_PERIOD (LPTIM_CLOCK_FREQ / CONFIG_SYS_CLOCK_TICKS_PER_SEC)

static struct k_spinlock lock;
static LPTIM_HandleTypeDef LPTIM_Handler;

void sys_clock_set_timeout(int32_t ticks, bool idle)
{
    ARG_UNUSED(idle);
    k_spinlock_key_t key = k_spin_lock(&lock);

    if (ticks == K_TICKS_FOREVER) {
        /* Disable LPTIM interrupt */
        __HAL_LPTIM_DISABLE_IT(&LPTIM_Handler, LPTIM_IT_ARRM);
    } else {
        /* Set the autoreload value */
        __HAL_LPTIM_AUTORELOAD_SET(&LPTIM_Handler, ticks * LPTIM_PERIOD);
        /* Enable LPTIM interrupt */
        __HAL_LPTIM_ENABLE_IT(&LPTIM_Handler, LPTIM_IT_ARRM);
    }

    k_spin_unlock(&lock, key);
}

uint32_t sys_clock_elapsed(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    uint32_t count = __HAL_LPTIM_GET_COUNTER(&LPTIM_Handler);
    k_spin_unlock(&lock, key);
    return count / LPTIM_PERIOD;
}

uint32_t sys_clock_cycle_get_32(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    uint32_t count = __HAL_LPTIM_GET_COUNTER(&LPTIM_Handler);
    k_spin_unlock(&lock, key);
    return count;
}

uint64_t sys_clock_cycle_get_64(void)
{
    return (uint64_t)sys_clock_cycle_get_32();
}

void sys_clock_disable(void)
{
    k_spinlock_key_t key = k_spin_lock(&lock);
    __HAL_LPTIM_DISABLE(&LPTIM_Handler);
    k_spin_unlock(&lock, key);
}

static void HAL_LPTIM_IRQHandler(void)
{
    /* Clear interrupt flag */
    __HAL_LPTIM_CLEAR_FLAG(&LPTIM_Handler, LPTIM_FLAG_ARRM);

    if (IS_ENABLED(CONFIG_TICKLESS_KERNEL)) {
        /* For tickless kernel, announce the elapsed ticks */
        uint32_t count = __HAL_LPTIM_GET_COUNTER(&LPTIM_Handler);
        sys_clock_announce(count / LPTIM_PERIOD);
    } else {
        /* For non-tickless kernel, announce one tick */
        sys_clock_announce(1);
    }
}

static int sys_clock_driver_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    int ret;

    /* Initialize LPTIM with default configuration */
    HAL_LPTIM_InitDefault(&LPTIM_Handler);

    /* Configure LPTIM */
    LPTIM_Handler.Init.Prescaler = LPTIM_PRESCALER;
    LPTIM_Handler.Init.CounterSource = LPTIM_COUNTERSOURCE_INTERNAL;
    LPTIM_Handler.Init.TriggerSource = LPTIM_TRIGSOURCE_SOFTWARE;
    LPTIM_Handler.Init.Clock.Source = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
    LPTIM_Handler.Init.Clock.Prescaler = LPTIM_CLOCK_PRESCALER_DIV1;
    LPTIM_Handler.Init.UltraLowPowerClock.Polarity = LPTIM_CLOCKPOLARITY_RISING;
    LPTIM_Handler.Init.UltraLowPowerClock.SampleTime = LPTIM_CLOCKSAMPLETIME_DIRECTTRANSITION;
    LPTIM_Handler.Init.Trigger.Source = LPTIM_TRIGSOURCE_SOFTWARE;
    LPTIM_Handler.Init.Trigger.ActiveEdge = LPTIM_ACTIVEEDGE_RISING;
    LPTIM_Handler.Init.Trigger.SampleTime = LPTIM_TRIGSAMPLETIME_DIRECTTRANSITION;
    LPTIM_Handler.Init.OutputPolarity = LPTIM_OUTPUTPOLARITY_HIGH;
    LPTIM_Handler.Init.UpdateMode = LPTIM_UPDATE_IMMEDIATE;
    LPTIM_Handler.Init.CounterMode = LPTIM_COUNTERMODE_UP;

    ret = HAL_LPTIM_Init(&LPTIM_Handler);
    if (ret != HAL_OK) {
        return -EIO;
    }

    /* Set initial period */
    __HAL_LPTIM_AUTORELOAD_SET(&LPTIM_Handler, LPTIM_PERIOD);

    /* Enable LPTIM */
    __HAL_LPTIM_ENABLE(&LPTIM_Handler);

    /* Start LPTIM in continuous mode */
    __HAL_LPTIM_START_CONTINUOUS(&LPTIM_Handler);

    /* Configure and enable interrupt */
    IRQ_CONNECT(DT_INST_IRQN(0), DT_INST_IRQ(0, priority),
                HAL_LPTIM_IRQHandler, NULL, 0);
    irq_enable(DT_INST_IRQN(0));

    return 0;
}

SYS_INIT(sys_clock_driver_init, PRE_KERNEL_2,
         CONFIG_SYSTEM_CLOCK_INIT_PRIORITY);