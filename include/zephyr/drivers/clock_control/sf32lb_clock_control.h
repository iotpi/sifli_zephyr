#ifndef _ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_SF32LB_CLOCK_CONTROL_H_
#define _ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_SF32LB_CLOCK_CONTROL_H_

#if DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(clk_hxt48m), fixed_clock, okay)
#define SF32LB_HXT48M_ENABLED 1
#define SF32LB_HXT48M_FREQ DT_PROP(DT_NODELABEL(clk_hxt48m), clock_frequency)
#elif DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(clk_hxt48m), sifli_sf32lb_hxt_clock, okay)
#define SF32LB_HXT48M_ENABLED 1
#define SF32LB_HXT48M_FREQ DT_PROP(DT_NODELABEL(clk_hxt48m), clock_frequency)
#endif

#if DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(clk_lxt32k), fixed_clock, okay)
#define SF32LB_LXT32K_ENABLED 1
#endif

#if DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(rcc), sifli_rcc, okay)
#define SF32_RCC_ENABLED 1

#endif

#if DT_NODE_HAS_COMPAT_STATUS(DT_NODELABEL(clk_sys), sifli_sys_clock, okay)
#define SF32_SYS_CLOCK_ENABLED 1
#define SF32_SCLK_HDIV    DT_PROP(DT_NODELABEL(clk_sys), hdiv)
#define SF32_SCLK_PDIV1   DT_PROP(DT_NODELABEL(clk_sys), pdiv1)
#define SF32_SCLK_PDIV2   DT_PROP(DT_NODELABEL(clk_sys), pdiv2)
#endif

struct sf32lb_rcc_clock {
    uint32_t reg;
    uint32_t bit;
};

#endif /* _ZEPHYR_INCLUDE_DRIVERS_CLOCK_CONTROL_SFLB32_CLOCK_CONTROL_H_ */
