#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_amp.h"
#include "esp_amp_platform.h"
#include "hal/gpio_ll.h"
#include "hal/rmt_hal.h"
#include "hal/rmt_ll.h"
#include "soc/gpio_num.h"
#include "soc/gpio_periph.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/rmt_periph.h"
#include "soc/soc.h"

#define TEST_RMT_CHANNEL 0
#define TEST_RMT_GROUP 0
#define TEST_GPIO 8
#define TEST_SYMBOLS_PER_CHANNEL 48
#define TEST_NUM_BITS 24
#define TEST_REPEATS 12
#define TEST_TX_LIMIT 48
#define TEST_TIMEOUT_US 10000

typedef struct {
    rmt_symbol_word_t symbols[TEST_SYMBOLS_PER_CHANNEL];
} test_rmt_channel_mem_t;

typedef struct {
    test_rmt_channel_mem_t channels[4];
} test_rmt_mem_t;

extern test_rmt_mem_t RMTMEM;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_color_t;

static const rmt_symbol_word_t s_bit0 = {
    .level0 = 1,
    .duration0 = 12,
    .level1 = 0,
    .duration1 = 36,
};

static const rmt_symbol_word_t s_bit1 = {
    .level0 = 1,
    .duration0 = 36,
    .level1 = 0,
    .duration1 = 12,
};

static uint32_t s_random_state = 0x6d2b79f5;

static uint32_t next_random(void)
{
    uint32_t value = s_random_state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    s_random_state = value;
    return value;
}

static uint32_t color_distance(const rgb_color_t *a, const rgb_color_t *b)
{
    int red = (int)a->red - b->red;
    int green = (int)a->green - b->green;
    int blue = (int)a->blue - b->blue;
    if (red < 0) {
        red = -red;
    }
    if (green < 0) {
        green = -green;
    }
    if (blue < 0) {
        blue = -blue;
    }
    return (uint32_t)(red + green + blue);
}

static rgb_color_t next_color(const rgb_color_t *previous, bool have_previous)
{
    rgb_color_t color;
    do {
        color.red = (uint8_t)(next_random() >> 24);
        color.green = (uint8_t)(next_random() >> 24);
        color.blue = (uint8_t)(next_random() >> 24);
    } while (have_previous && color_distance(&color, previous) < 240);
    return color;
}

static void configure_gpio(void)
{
    gpio_ll_iomux_func_sel(GPIO_PIN_MUX_REG[TEST_GPIO], PIN_FUNC_GPIO);
    gpio_ll_output_enable(&GPIO, TEST_GPIO);
    gpio_ll_pullup_dis(&GPIO, TEST_GPIO);
    gpio_ll_input_disable(&GPIO, TEST_GPIO);
    gpio_ll_pulldown_dis(&GPIO, TEST_GPIO);
    gpio_ll_od_disable(&GPIO, TEST_GPIO);
    gpio_ll_sleep_sel_dis(&GPIO, TEST_GPIO);
    REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + (TEST_GPIO * 4), RMT_SIG_OUT0_IDX);
}

static void configure_rmt(void)
{
    rmt_dev_t *rmt = &RMT;

    rmt_ll_enable_bus_clock(TEST_RMT_GROUP, true);
    rmt_ll_reset_register(TEST_RMT_GROUP);
    rmt_ll_mem_power_by_pmu(rmt);
    rmt_ll_enable_mem_access_nonfifo(rmt, true);
    rmt_ll_enable_interrupt(rmt, UINT32_MAX, false);
    rmt_ll_clear_interrupt_status(rmt, UINT32_MAX);

    rmt_ll_set_group_clock_src(rmt, TEST_RMT_CHANNEL, RMT_CLK_SRC_XTAL, 1, 1, 0);
    rmt_ll_enable_group_clock(rmt, true);
    rmt_ll_tx_reset_channels_clock_div(rmt, 1 << TEST_RMT_CHANNEL);
    rmt_ll_tx_set_channel_clock_div(rmt, TEST_RMT_CHANNEL, 1);
    rmt_ll_tx_set_mem_blocks(rmt, TEST_RMT_CHANNEL, 1);
    rmt_ll_tx_set_limit(rmt, TEST_RMT_CHANNEL, TEST_TX_LIMIT);
    rmt_ll_tx_enable_carrier_modulation(rmt, TEST_RMT_CHANNEL, false);
    rmt_ll_tx_fix_idle_level(rmt, TEST_RMT_CHANNEL, 0, true);
    rmt_ll_tx_enable_wrap(rmt, TEST_RMT_CHANNEL, false);

    configure_gpio();
}

static bool wait_for_idle(void)
{
    rmt_dev_t *rmt = &RMT;
    uint32_t waited_us = 0;

    if (rmt->chnstatus[TEST_RMT_CHANNEL].state_chn == 0) {
        return true;
    }

    rmt_ll_tx_stop(rmt, TEST_RMT_CHANNEL);
    while (rmt->chnstatus[TEST_RMT_CHANNEL].state_chn != 0 &&
           waited_us < TEST_TIMEOUT_US) {
        esp_amp_platform_delay_us(1);
        waited_us++;
    }
    return rmt->chnstatus[TEST_RMT_CHANNEL].state_chn == 0;
}

static void encode_color(const rgb_color_t *color, uint8_t forced_last_bit)
{
    uint8_t grb[3] = {
        color->green,
        color->red,
        (uint8_t)((color->blue & 0xfe) | forced_last_bit),
    };

    for (size_t bit = 0; bit < TEST_NUM_BITS; bit++) {
        uint8_t mask = (uint8_t)(1U << (7 - (bit % 8)));
        bool value = (grb[bit / 8] & mask) != 0;
        RMTMEM.channels[TEST_RMT_CHANNEL].symbols[bit] = value ? s_bit1 : s_bit0;
    }
    RMTMEM.channels[TEST_RMT_CHANNEL].symbols[TEST_NUM_BITS] = (rmt_symbol_word_t){ .val = 0 };
}

static uint32_t transmit_color(const rgb_color_t *color, uint8_t forced_last_bit,
                               uint32_t iteration)
{
    rmt_dev_t *rmt = &RMT;
    const uint32_t channel = TEST_RMT_CHANNEL;
    const uint32_t loop_end = RMT_LL_EVENT_TX_LOOP_END(channel);
    const uint32_t tx_done = RMT_LL_EVENT_TX_DONE(channel);
    const uint32_t tx_error = RMT_LL_EVENT_TX_ERROR(channel);
    const uint32_t terminal_events = loop_end | tx_done | tx_error;
    const uint8_t wire_blue = (uint8_t)((color->blue & 0xfe) | forced_last_bit);
    const uint8_t grb[3] = { color->green, color->red, wire_blue };

    if (!wait_for_idle()) {
        printf("RMT idle timeout: status=0x%08x\n",
               (unsigned)rmt->chnstatus[channel].val);
        return 0;
    }

    encode_color(color, forced_last_bit);
    rmt_ll_tx_reset_pointer(rmt, channel);
    rmt_ll_tx_reset_loop_count(rmt, channel);
    rmt_ll_tx_enable_loop(rmt, channel, true);
    rmt_ll_tx_enable_loop_count(rmt, channel, true);
    rmt_ll_tx_set_loop_count(rmt, channel, TEST_REPEATS);
    rmt_ll_tx_enable_loop_autostop(rmt, channel, true);
    rmt_ll_clear_interrupt_status(rmt, RMT_LL_EVENT_TX_MASK(channel) | tx_error);

    printf("RMT test #%u RGB=%02x%02x%02x GRB=%02x%02x%02x "
           "forced_last_bit=%u sym23=0x%08x EOF=0x%08x\n",
           (unsigned)iteration,
           color->red, color->green, color->blue,
           grb[0], grb[1], grb[2],
           forced_last_bit,
           (unsigned)RMTMEM.channels[channel].symbols[TEST_NUM_BITS - 1].val,
           (unsigned)RMTMEM.channels[channel].symbols[TEST_NUM_BITS].val);

    printf("RMT symbols:");
    for (size_t i = 0; i <= TEST_NUM_BITS; i++) {
        printf(" %u:0x%08x", (unsigned)i,
               (unsigned)RMTMEM.channels[channel].symbols[i].val);
    }
    printf("\n");

    printf("RMT start: conf=0x%08x tx_lim=0x%08x status=0x%08x\n",
           (unsigned)rmt->chnconf0[channel].val,
           (unsigned)rmt->chn_tx_lim[channel].val,
           (unsigned)rmt->chnstatus[channel].val);
    rmt_ll_tx_start(rmt, channel);

    uint32_t events = 0;
    uint32_t waited_us = 0;
    while (events == 0 && waited_us < TEST_TIMEOUT_US) {
        events = rmt_ll_tx_get_interrupt_status_raw(rmt, channel) & terminal_events;
        if (events == 0) {
            esp_amp_platform_delay_us(1);
            waited_us++;
        }
    }

    printf("RMT result=%s events=0x%08x status=0x%08x "
           "tx_lim=0x%08x int_raw=0x%08x\n",
           (events & loop_end) ? "loop_end" :
           (events & tx_done) ? "tx_done" :
           (events & tx_error) ? "tx_error" : "timeout",
           (unsigned)events,
           (unsigned)rmt->chnstatus[channel].val,
           (unsigned)rmt->chn_tx_lim[channel].val,
           (unsigned)rmt_ll_tx_get_interrupt_status_raw(rmt, channel));
    return events;
}

int main(void)
{
    if (esp_amp_init() != 0) {
        return 1;
    }

    printf("RMT loop errata test: GPIO=%u, repeats=%u, interval=1s\n",
           TEST_GPIO, TEST_REPEATS);
    configure_rmt();

    rgb_color_t previous = { 0 };
    bool have_previous = false;
    uint8_t forced_last_bit = 0;
    uint32_t iteration = 0;

    while (true) {
        rgb_color_t color = next_color(&previous, have_previous);
        color.blue = (uint8_t)((color.blue & 0xfe) | forced_last_bit);
        transmit_color(&color, forced_last_bit, iteration++);
        previous = color;
        have_previous = true;
        forced_last_bit ^= 1;
        esp_amp_platform_delay_ms(1000);
    }
}
