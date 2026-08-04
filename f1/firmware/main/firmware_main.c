/* firmware: Fred the robot. AMG8833 thermal camera, LSM6DSOX IMU and
 * INA219 power monitor on one I2C bus, DRV8833 motors, the DevKit's
 * onboard RGB status LED, and a serial test console (type '?' in
 * idf.py monitor). */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AMG_SDA_GPIO    8
#define AMG_SCL_GPIO    9
#define AMG_I2C_ADDR    0x69
#define AMG_REG_PCTL    0x00   /* power control: 0x00 = normal mode */
#define AMG_REG_RST     0x01   /* reset: 0x3F = initial reset */
#define AMG_REG_FPSC    0x02   /* frame rate: 0x00 = 10 fps */
#define AMG_REG_INTC    0x03   /* interrupt control: 0x00 = disabled */
#define AMG_REG_PIXELS  0x80   /* 64 pixels x 2 bytes, little-endian */

#define INA_I2C_ADDR    0x40
#define INA_REG_CONFIG  0x00   /* 0x399F = 32 V range, ±320 mV PGA, 12-bit */
#define INA_REG_SHUNT   0x01   /* signed, 10 uV per LSB across the 0.1R shunt */
#define INA_REG_BUS     0x02   /* bits 15..3, 4 mV per LSB */

#define LSM_I2C_ADDR     0x6A
#define LSM_REG_WHOAMI   0x0F  /* reads 0x6C */
#define LSM_REG_CTRL1_XL 0x10  /* 0x40 = accel 104 Hz, ±2 g */
#define LSM_REG_CTRL2_G  0x11  /* 0x44 = gyro 104 Hz, ±500 dps */
#define LSM_REG_OUTX_L_G 0x22  /* 12 bytes: gyro xyz then accel xyz, LE */

/* Deliberately scrambled vs the DRV8833 pin names: the A channel is
 * soldered to the right motor and B to the left, and both motors have
 * inverted polarity, so we un-cross and un-invert them here. */
#define MOTOR_L_IN1_GPIO  7    /* DRV8833 BIN2 */
#define MOTOR_L_IN2_GPIO  6    /* DRV8833 BIN1 */
#define MOTOR_R_IN1_GPIO  5    /* DRV8833 AIN2 */
#define MOTOR_R_IN2_GPIO  4    /* DRV8833 AIN1 */
#define DRV_SLP_GPIO      10   /* DRV8833 nSLEEP: high = enabled */

#define PWM_FREQ_HZ     25000  /* above audible, well under DRV8833's max */
#define MOTOR_MIN_PCT   25     /* measured deadband (cal 20260804133108): no
                                  motion at 20, reliable from rest at 25;
                                  below it wheels stall-or-creep at random */
#define PWM_RES         LEDC_TIMER_10_BIT
#define PWM_MAX         (1 << 10)   /* LEDC duty range is [0, 2^res] */

#define RGB_GPIO        38     /* DevKitC-1 v1.1 onboard WS2812; v1.0 boards use 48 */

/* Status colours, dim enough to look at — the LED is blinding at full duty.
 * Red = booting or a check failed, green = all well, blue = performing. */
#define RGB_RED         32, 0, 0
#define RGB_GREEN       0, 32, 0
#define RGB_BLUE        0, 0, 48

#define MOTOR_L_IN1_CH  LEDC_CHANNEL_0
#define MOTOR_L_IN2_CH  LEDC_CHANNEL_1
#define MOTOR_R_IN1_CH  LEDC_CHANNEL_2
#define MOTOR_R_IN2_CH  LEDC_CHANNEL_3

static i2c_master_dev_handle_t amg, ina, lsm;
static bool amg_ok, ina_ok, lsm_ok;

static i2c_master_bus_handle_t i2c_bus;

static void i2c_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = -1,
        .sda_io_num = AMG_SDA_GPIO,
        .scl_io_num = AMG_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
}

static i2c_master_dev_handle_t i2c_add(uint8_t addr)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 400 * 1000,
    };
    i2c_master_dev_handle_t dev = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &dev));
    return dev;
}

static esp_err_t amg_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(amg, buf, sizeof(buf), 100);
}

static void amg_init(void)
{
    amg = i2c_add(AMG_I2C_ADDR);

    /* Non-fatal: a missing sensor must not stop motor testing. */
    if (amg_write_reg(AMG_REG_PCTL, 0x00) != ESP_OK ||
        amg_write_reg(AMG_REG_RST, 0x3F) != ESP_OK ||
        amg_write_reg(AMG_REG_INTC, 0x00) != ESP_OK ||
        amg_write_reg(AMG_REG_FPSC, 0x00) != ESP_OK) {
        printf("AMG8833 not responding; thermal readings disabled\n");
        amg_ok = false;
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    amg_ok = true;
}

/* All 64 pixels, sign-extended raw counts (0.25 C per LSB). */
static esp_err_t amg_read_pixels(int16_t px[64])
{
    uint8_t reg = AMG_REG_PIXELS;
    uint8_t raw[128];
    esp_err_t err = i2c_master_transmit_receive(amg, &reg, 1, raw, sizeof(raw), 100);
    if (err != ESP_OK) {
        return err;
    }
    for (int i = 0; i < 64; i++) {
        int v = (raw[2 * i] | (raw[2 * i + 1] << 8)) & 0x0FFF;
        if (v & 0x800) {
            v -= 0x1000;   /* 12-bit two's complement */
        }
        px[i] = v;
    }
    return ESP_OK;
}

/* Mean of all 64 pixels in degrees C. */
static esp_err_t amg_read_mean(float *mean)
{
    int16_t px[64];
    esp_err_t err = amg_read_pixels(px);
    if (err != ESP_OK) {
        return err;
    }
    int sum = 0;
    for (int i = 0; i < 64; i++) {
        sum += px[i];
    }
    *mean = sum * 0.25f / 64.0f;
    return ESP_OK;
}

static esp_err_t ina_read_reg(uint8_t reg, uint16_t *val)
{
    uint8_t raw[2];
    esp_err_t err = i2c_master_transmit_receive(ina, &reg, 1, raw, sizeof(raw), 100);
    if (err != ESP_OK) {
        return err;
    }
    *val = (raw[0] << 8) | raw[1];   /* registers are big-endian */
    return ESP_OK;
}

static void ina_init(void)
{
    ina = i2c_add(INA_I2C_ADDR);
    uint8_t cfg[3] = { INA_REG_CONFIG, 0x39, 0x9F };
    if (i2c_master_transmit(ina, cfg, sizeof(cfg), 100) != ESP_OK) {
        printf("INA219 not responding; power readings disabled\n");
        return;
    }
    ina_ok = true;
}

/* Pack voltage in volts and current in mA, positive when discharging. */
static esp_err_t ina_read(float *volts, float *milliamps)
{
    uint16_t bus_reg, shunt_reg;
    esp_err_t err = ina_read_reg(INA_REG_BUS, &bus_reg);
    if (err == ESP_OK) {
        err = ina_read_reg(INA_REG_SHUNT, &shunt_reg);
    }
    if (err != ESP_OK) {
        return err;
    }
    *volts = (bus_reg >> 3) * 0.004f;
    *milliamps = (int16_t)shunt_reg * 0.1f;   /* 10 uV / 0.1R = 100 uA per LSB */
    return ESP_OK;
}

static void lsm_init(void)
{
    lsm = i2c_add(LSM_I2C_ADDR);
    uint8_t reg = LSM_REG_WHOAMI, id = 0;
    uint8_t xl_cfg[2] = { LSM_REG_CTRL1_XL, 0x40 };
    uint8_t g_cfg[2] = { LSM_REG_CTRL2_G, 0x44 };
    if (i2c_master_transmit_receive(lsm, &reg, 1, &id, 1, 100) != ESP_OK ||
        id != 0x6C ||
        i2c_master_transmit(lsm, xl_cfg, sizeof(xl_cfg), 100) != ESP_OK ||
        i2c_master_transmit(lsm, g_cfg, sizeof(g_cfg), 100) != ESP_OK) {
        printf("LSM6DSOX not responding; motion readings disabled\n");
        return;
    }
    lsm_ok = true;
}

/* Gyro in degrees/s and accel in g, both x/y/z. */
static esp_err_t lsm_read(float dps[3], float g[3])
{
    uint8_t reg = LSM_REG_OUTX_L_G;
    uint8_t raw[12];
    esp_err_t err = i2c_master_transmit_receive(lsm, &reg, 1, raw, sizeof(raw), 100);
    if (err != ESP_OK) {
        return err;
    }
    for (int i = 0; i < 3; i++) {
        dps[i] = (int16_t)(raw[2 * i] | (raw[2 * i + 1] << 8)) * 0.0175f;      /* 17.5 mdps/LSB at ±500 dps */
        g[i] = (int16_t)(raw[6 + 2 * i] | (raw[7 + 2 * i] << 8)) * 0.000061f;  /* 0.061 mg/LSB at ±2 g */
    }
    return ESP_OK;
}

static rmt_channel_handle_t rgb_chan;
static rmt_encoder_handle_t rgb_enc;

static void rgb_init(void)
{
    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num = RGB_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,   /* 0.1 us ticks */
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &rgb_chan));

    /* WS2812 bit timing in those ticks: 0 = 0.3 us high / 0.9 us low,
     * 1 = 0.9 us high / 0.3 us low. */
    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },
        .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &rgb_enc));
    ESP_ERROR_CHECK(rmt_enable(rgb_chan));
}

static void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t grb[3] = { g, r, b };   /* WS2812 byte order */
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    ESP_ERROR_CHECK(rmt_transmit(rgb_chan, rgb_enc, grb, sizeof(grb), &tx_cfg));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(rgb_chan, 100));
    esp_rom_delay_us(60);   /* latch gap so a back-to-back frame isn't swallowed */
}

static void pwm_channel_init(ledc_channel_t ch, int gpio)
{
    ledc_channel_config_t cfg = {
        .gpio_num = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = ch,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&cfg));
}

static void pwm_set(ledc_channel_t ch, uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, ch));
}

/* pct is -100..100; 0 coasts. Slow-decay drive: the leading pin is held
 * high and the other PWM'd, which keeps torque at low speeds. */
static void motor_set(ledc_channel_t in1, ledc_channel_t in2, int pct)
{
    if (pct > 100) {
        pct = 100;
    } else if (pct < -100) {
        pct = -100;
    }
    /* Remap nonzero magnitudes onto the live range above the deadband,
     * so small commands crawl instead of gambling on breakaway. */
    int mag = abs(pct);
    if (mag > 0) {
        mag = MOTOR_MIN_PCT + mag * (100 - MOTOR_MIN_PCT) / 100;
    }
    uint32_t duty = PWM_MAX * mag / 100;
    if (pct > 0) {
        pwm_set(in1, PWM_MAX);
        pwm_set(in2, PWM_MAX - duty);
    } else if (pct < 0) {
        pwm_set(in1, PWM_MAX - duty);
        pwm_set(in2, PWM_MAX);
    } else {
        pwm_set(in1, 0);
        pwm_set(in2, 0);
    }
}

/* True when the pack is supplying current. With the pack off and USB in,
 * the 6V rail is back-fed through the DevKit's diode and the pack shunt
 * carries nothing — running motors then would pull amps through that
 * diode (see schematic.md). The pack covers the ESP32's idle draw
 * whenever it's on (measured 43-59 mA untethered, ~0 on USB), so a
 * near-zero shunt reading means USB only. */
static bool pack_live(void)
{
    float volts, ma;
    if (!ina_ok || ina_read(&volts, &ma) != ESP_OK) {
        return true;   /* can't tell — assume the pack is on */
    }
    return ma > 20.0f;
}

/* No two TT motors are matched: the trim is added to the left duty
 * (sign-aware, so it corrects magnitude in reverse too) whenever both
 * wheels are driven. Set by open-loop console test, not by fitting hunt
 * logs — those commands come from steering feedback and the fit's
 * intercept is biased (it once said -6, which itself caused a left
 * veer). Measured: actual duties 49/50 drive straight. */
#define DRIVE_TRIM_PCT  -1

static int16_t cmd_left, cmd_right;   /* commanded duties pre-trim, for the log */

static void drive(int left_pct, int right_pct)
{
    cmd_left = left_pct;
    cmd_right = right_pct;
    if (left_pct != 0 && right_pct != 0) {
        left_pct += (left_pct > 0) ? DRIVE_TRIM_PCT : -DRIVE_TRIM_PCT;
    }
    if (!pack_live()) {
        printf("motors (traced, usb power only): left %d right %d\n",
               left_pct, right_pct);
        return;
    }
    motor_set(MOTOR_L_IN1_CH, MOTOR_L_IN2_CH, left_pct);
    motor_set(MOTOR_R_IN1_CH, MOTOR_R_IN2_CH, right_pct);
}

/* "I'm alive and all is well": wiggle on the spot with blue winks on the
 * beats, then settle to green. One function per performance until there
 * are enough of them to be worth a dispatcher. */
static void perform_alive(void)
{
    for (int i = 0; i < 3; i++) {
        rgb_set(RGB_BLUE);
        drive(60, -60);   /* above the ~30% standstill deadband */
        vTaskDelay(pdMS_TO_TICKS(150));
        rgb_set(RGB_GREEN);
        drive(-60, 60);
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    drive(0, 0);
    rgb_set(RGB_GREEN);
}

/* "Found you!": a quick excited shimmy, used when the hunt spots heat. */
static void perform_found(void)
{
    for (int i = 0; i < 2; i++) {
        drive(40, -40);
        vTaskDelay(pdMS_TO_TICKS(120));
        drive(-40, 40);
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    drive(0, 0);
}

/* One blue wink per second — the fuse for delayed starts. */
static void countdown_winks(int secs)
{
    for (int i = 0; i < secs; i++) {
        rgb_set(RGB_BLUE);
        vTaskDelay(pdMS_TO_TICKS(200));
        rgb_set(0, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}

static void motors_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = PWM_RES,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    pwm_channel_init(MOTOR_L_IN1_CH, MOTOR_L_IN1_GPIO);
    pwm_channel_init(MOTOR_L_IN2_CH, MOTOR_L_IN2_GPIO);
    pwm_channel_init(MOTOR_R_IN1_CH, MOTOR_R_IN1_GPIO);
    pwm_channel_init(MOTOR_R_IN2_CH, MOTOR_R_IN2_GPIO);

    gpio_config_t slp_cfg = {
        .pin_bit_mask = 1ULL << DRV_SLP_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&slp_cfg));
    gpio_set_level(DRV_SLP_GPIO, 1);
}

#define TICK_HZ 10   /* the sensor/behaviour heartbeat */

/* Bump/stuck detection, evaluated every tick from the IMU while the
 * motors are commanded. Two events: a jolt (drove into something) and
 * stuck (commanded but not actually moving — gearbox vibration is the
 * witness: <2 mg at rest, >29 mg rolling, cal 20260804133108). All
 * first guesses — tune from recorded logs of deliberate collisions. */
#define BUMP_JOLT_G    0.20f   /* accel deviation from 1 g = collision */
#define STUCK_VIB_G    0.010f  /* below this vibration while commanded = stuck */
#define STUCK_GRACE_MS 1000    /* let wheels spin up before calling stuck */

/* Escape reflex: on bump/stuck, reverse for a moment, then a gyro-
 * measured about-turn — 180 plus jitter, since exact 180s ping-pong in
 * corners — and carry on with the pre-bump command. Bumps arriving
 * right after an escape mean still trapped; after a few tries he stops
 * rather than thrash. Toggle with b; the cal script disables it. */
#define ESC_REV_MS       1000
#define ESC_TURN_PCT     40    /* pre-remap spin duty for the about-turn */
#define ESC_TURN_MIN_DEG 150
#define ESC_TURN_MAX_DEG 210
#define ESC_TIMEOUT_MS   4500  /* turn safety net */
#define ESC_RETRY_S      3     /* a bump this soon after escaping = trapped */
#define ESC_MAX_TRIES    4

typedef enum { ESC_NONE = 0, ESC_REV, ESC_TURN } esc_state_t;

static esc_state_t esc_state;
static bool esc_enable = true;
static int64_t esc_phase_end;
static float esc_yaw, esc_yaw_target;
static int esc_turn_sign;
static int16_t esc_resume_l, esc_resume_r;
static int esc_tries;
static int64_t esc_done_at;

static float vib_ema;             /* EMA of |d(accel magnitude)| per tick */
static float amag_prev = 1.0f;
static int64_t moving_since;
static int64_t bump_led_until;

/* Returns 0 quiet, 1 jolt, 2 stuck; logged per tick for tuning. */
static uint8_t bump_check(const float g[3])
{
    float amag = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]);
    float d = fabsf(amag - amag_prev);
    amag_prev = amag;
    vib_ema += 0.2f * (d - vib_ema);

    if (esc_state != ESC_NONE) {
        return 0;   /* his own escape jerks must not retrigger */
    }
    int64_t now = esp_timer_get_time();
    bool cmd_moving = (cmd_left != 0 || cmd_right != 0) && pack_live();
    if (!cmd_moving) {
        moving_since = 0;
    } else if (moving_since == 0) {
        moving_since = now;
        vib_ema = STUCK_VIB_G * 4;   /* fresh benefit of the doubt */
    }

    uint8_t event = 0;
    if (cmd_moving && fabsf(amag - 1.0f) > BUMP_JOLT_G) {
        event = 1;
    } else if (cmd_moving && now - moving_since > STUCK_GRACE_MS * 1000 &&
               vib_ema < STUCK_VIB_G) {
        event = 2;
    }
    if (event) {
        rgb_set(RGB_RED);   /* a visible ouch, without stopping anything */
        bump_led_until = now + 500 * 1000;
        printf(event == 1 ? "bump!\n" : "stuck!\n");
    } else if (bump_led_until != 0 && now > bump_led_until) {
        rgb_set(RGB_GREEN);
        bump_led_until = 0;
    }
    return event;
}

static void esc_step(uint8_t bump, const float dps[3])
{
    int64_t now = esp_timer_get_time();
    switch (esc_state) {
    case ESC_NONE:
        if (bump == 0) {
            return;
        }
        esc_tries = (esc_done_at != 0 &&
                     now - esc_done_at < ESC_RETRY_S * 1000000LL)
                        ? esc_tries + 1 : 1;
        if (esc_tries > ESC_MAX_TRIES) {
            drive(0, 0);
            printf("escape: still trapped after %d tries, stopping\n",
                   ESC_MAX_TRIES);
            return;
        }
        esc_resume_l = cmd_left;
        esc_resume_r = cmd_right;
        /* back away from the bump — or forward, if he was reversing */
        int back = (esc_resume_l + esc_resume_r >= 0) ? -1 : 1;
        drive(back * 40, back * 40);
        esc_state = ESC_REV;
        esc_phase_end = now + ESC_REV_MS * 1000;
        break;

    case ESC_REV:
        if (now < esc_phase_end) {
            return;
        }
        esc_yaw = 0;
        esc_yaw_target = ESC_TURN_MIN_DEG +
            (esp_random() % (ESC_TURN_MAX_DEG - ESC_TURN_MIN_DEG + 1));
        esc_turn_sign = (esp_random() & 1) ? 1 : -1;
        drive(esc_turn_sign * ESC_TURN_PCT, -esc_turn_sign * ESC_TURN_PCT);
        esc_state = ESC_TURN;
        esc_phase_end = now + ESC_TIMEOUT_MS * 1000;
        break;

    case ESC_TURN:
        esc_yaw += dps[2] * (1.0f / TICK_HZ);
        if (fabsf(esc_yaw) < esc_yaw_target && now < esc_phase_end) {
            return;
        }
        drive(esc_resume_l, esc_resume_r);   /* carry on as before */
        esc_state = ESC_NONE;
        esc_done_at = now;
        printf("escape: reversed, turned %.0f deg, carrying on\n",
               fabsf(esc_yaw));
        break;
    }
}

/* Recorder: 10 Hz snapshots of every sensor plus the commanded motor
 * duties and any bump/stuck event, into a PSRAM ring for bench
 * analysis (r to record, d to dump as CSV). PSRAM is wiped by reset —
 * and `idf.py monitor` resets the chip on connect, so retrieve
 * untethered runs with --no-reset. */
#define REC_SPIRAM_N   36000   /* ~1 h at 10 Hz, ~6 MB of the 8 MB PSRAM */
#define REC_INTERNAL_N 600     /* ~1 min fallback if PSRAM is absent */

typedef struct {
    uint32_t ms;
    int16_t left, right;
    int16_t px[64];
    float dps[3], g[3];
    float volts, ma;
    uint8_t bump;               /* 0 quiet, 1 jolt, 2 stuck */
} rec_t;

static rec_t *rec_buf;
static int rec_cap, rec_head, rec_len;
static volatile bool rec_on;

/* One 10 Hz heartbeat: read the IMU, run bump detection, then snapshot
 * everything for the recorder. */
static void tick_task(void *arg)
{
    TickType_t wake = xTaskGetTickCount();
    while (1) {
        xTaskDelayUntil(&wake, pdMS_TO_TICKS(1000 / TICK_HZ));
        float dps[3], g[3];
        bool imu_ok = lsm_ok && lsm_read(dps, g) == ESP_OK;
        uint8_t bump = imu_ok ? bump_check(g) : 0;
        if (imu_ok && esc_enable) {
            esc_step(bump, dps);
        }
        if (!rec_on || rec_cap == 0) {
            continue;
        }
        rec_t *r = &rec_buf[rec_head];
        memset(r, 0, sizeof(*r));   /* failed reads log as zeros */
        r->ms = esp_timer_get_time() / 1000;
        r->left = cmd_left;
        r->right = cmd_right;
        if (amg_ok) {
            amg_read_pixels(r->px);
        }
        if (imu_ok) {
            memcpy(r->dps, dps, sizeof(r->dps));
            memcpy(r->g, g, sizeof(r->g));
        }
        if (ina_ok) {
            ina_read(&r->volts, &r->ma);
        }
        r->bump = bump;
        rec_head = (rec_head + 1) % rec_cap;
        if (rec_len < rec_cap) {
            rec_len++;
        }
    }
}

static void tick_init(void)
{
    rec_cap = REC_SPIRAM_N;
    rec_buf = heap_caps_malloc(rec_cap * sizeof(rec_t), MALLOC_CAP_SPIRAM);
    if (rec_buf == NULL) {
        rec_cap = REC_INTERNAL_N;
        rec_buf = malloc(rec_cap * sizeof(rec_t));
    }
    if (rec_buf == NULL) {
        rec_cap = 0;   /* bump detection still needs the heartbeat */
        printf("no memory for the recorder; r/d disabled\n");
    }
    xTaskCreate(tick_task, "tick", 4096, NULL, 5, NULL);
}

static void rec_dump(void)
{
    if (rec_on) {
        printf("still recording; r to stop first\n");
        return;
    }
    printf("ms,left,right,volts,ma,bump,gx,gy,gz,ax,ay,az");
    for (int i = 0; i < 64; i++) {
        printf(",p%d", i);
    }
    printf("\n");
    for (int i = 0; i < rec_len; i++) {
        rec_t *r = &rec_buf[(rec_head - rec_len + i + rec_cap) % rec_cap];
        printf("%lu,%d,%d,%.2f,%.0f,%d,%.1f,%.1f,%.1f,%.3f,%.3f,%.3f",
               (unsigned long)r->ms, r->left, r->right, r->volts, r->ma,
               r->bump,
               r->dps[0], r->dps[1], r->dps[2], r->g[0], r->g[1], r->g[2]);
        for (int p = 0; p < 64; p++) {
            printf(",%.2f", r->px[p] * 0.25f);
        }
        printf("\n");
    }
    printf("# %d records\n", rec_len);
}

/* Calibration script: fixed open-loop drive segments meant to be
 * recorded (r) and analysed on the bench — trim, deadband, breakaway.
 * Each segment starts from standstill so every run exercises the
 * kick-from-rest behaviour, and forward runs are mirrored backward so
 * f1 roughly returns to where he started. Extend the table as needed. */
static const struct { int left, right; int ms; } cal_seq[] = {
    { 40, 40, 2500 },  { -40, -40, 2500 },
    { 50, 50, 2500 },  { -50, -50, 2500 },
    { 60, 60, 2500 },  { -60, -60, 2500 },
    { 30, 30, 1500 },  { -30, -30, 1500 },
    { 25, 25, 1500 },  { -25, -25, 1500 },
    { 20, 20, 1500 },  { -20, -20, 1500 },
    { 15, 15, 1500 },  { -15, -15, 1500 },
};

static void cal_run(int delay_s)
{
    bool esc_was = esc_enable;
    esc_enable = false;   /* breakaway jolts must not hijack the script */
    if (!rec_on) {
        printf("cal: note, not recording (r first to log the run)\n");
    }
    if (delay_s > 0) {
        printf("cal: starting in %d s\n", delay_s);
        countdown_winks(delay_s);
        rgb_set(RGB_GREEN);
    }
    for (int i = 0; i < sizeof(cal_seq) / sizeof(cal_seq[0]); i++) {
        printf("cal: left %d right %d for %d ms\n",
               cal_seq[i].left, cal_seq[i].right, cal_seq[i].ms);
        drive(cal_seq[i].left, cal_seq[i].right);
        vTaskDelay(pdMS_TO_TICKS(cal_seq[i].ms));
        drive(0, 0);
        vTaskDelay(pdMS_TO_TICKS(700));   /* settle, and mark the segment */
    }
    esc_enable = esc_was;
    printf("cal: done\n");
}

static void print_help(void)
{
    printf("commands:\n"
           "  m <left> <right>   set motor speeds, -100..100 (m 50 50)\n"
           "  s                  stop (coast)\n"
           "  z                  sleep the motor driver\n"
           "  w                  wake the motor driver\n"
           "  t                  read thermal mean\n"
           "  v                  read pack voltage and current\n"
           "  g                  read gyro and accelerometer\n"
           "  c [secs]           motor calibration script, after an optional\n"
           "                     delay to get him on the floor (r first)\n"
           "  p <which>          perform: a = I'm-alive, f = found-you\n"
           "  b                  bump escape reflex on/off (default on)\n"
           "  l <r> <g> <b>      set the RGB LED, 0..255 (l 32 0 0)\n"
           "  r                  record all sensors at 10 Hz, start/stop\n"
           "  d                  dump the recording as CSV\n"
           "  ?                  this help\n");
}

static void handle_line(char *line)
{
    int l, r, cr, cg, cb;
    char pc;
    if (sscanf(line, "m %d %d", &l, &r) == 2) {
        drive(l, r);
        printf("motors: left %d right %d\n", l, r);
    } else if (strcmp(line, "s") == 0) {
        drive(0, 0);
        printf("motors: stopped\n");
    } else if (strcmp(line, "z") == 0) {
        drive(0, 0);   /* wake to a known state, not mid-command speeds */
        gpio_set_level(DRV_SLP_GPIO, 0);
        printf("driver: asleep\n");
    } else if (strcmp(line, "w") == 0) {
        gpio_set_level(DRV_SLP_GPIO, 1);
        printf("driver: awake\n");
    } else if (strcmp(line, "t") == 0) {
        float mean;
        if (amg_ok && amg_read_mean(&mean) == ESP_OK) {
            printf("%.2f C\n", mean);
        } else {
            printf("sensor read failed\n");
        }
    } else if (strcmp(line, "v") == 0) {
        float volts, ma;
        if (ina_ok && ina_read(&volts, &ma) == ESP_OK) {
            printf("%.2f V, %.0f mA\n", volts, ma);
        } else {
            printf("power sensor read failed\n");
        }
    } else if (strcmp(line, "g") == 0) {
        float dps[3], g[3];
        if (lsm_ok && lsm_read(dps, g) == ESP_OK) {
            printf("gyro %7.1f %7.1f %7.1f dps  accel %6.2f %6.2f %6.2f g\n",
                   dps[0], dps[1], dps[2], g[0], g[1], g[2]);
        } else {
            printf("motion sensor read failed\n");
        }
    } else if (sscanf(line, "p %c", &pc) == 1) {
        if (pc == 'a') {
            perform_alive();
            printf("performance: done\n");
        } else if (pc == 'f') {
            perform_found();
            printf("performance: done\n");
        } else {
            printf("no such performance: %c\n", pc);
        }
    } else if (sscanf(line, "l %d %d %d", &cr, &cg, &cb) == 3) {
        rgb_set(cr, cg, cb);
        printf("led: %d %d %d\n", cr, cg, cb);
    } else if (strcmp(line, "r") == 0) {
        if (rec_cap == 0) {
            printf("recorder disabled (no memory)\n");
        } else if (rec_on) {
            rec_on = false;
            printf("recording stopped: %d records (d to dump)\n", rec_len);
        } else {
            rec_head = 0;
            rec_len = 0;   /* each run starts fresh */
            rec_on = true;
            printf("recording at %d Hz (r to stop)\n", TICK_HZ);
        }
    } else if (strcmp(line, "d") == 0) {
        rec_dump();
    } else if (strcmp(line, "b") == 0) {
        esc_enable = !esc_enable;
        printf("escape reflex: %s\n", esc_enable ? "on" : "off");
    } else if (line[0] == 'c' && (line[1] == '\0' || line[1] == ' ')) {
        int delay = 0;
        sscanf(line, "c %d", &delay);
        cal_run(delay);
    } else {
        print_help();
    }
}

void app_main(void)
{
    rgb_init();
    rgb_set(RGB_RED);   /* red until every check passes */

    motors_init();
    i2c_init();
    amg_init();
    ina_init();
    lsm_init();
    tick_init();
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    print_help();

    if (amg_ok && ina_ok && lsm_ok) {
        rgb_set(RGB_GREEN);
        perform_alive();
        printf("startup: all checks passed\n");
    } else {
        printf("startup: checks failed, staying red and still\n");
    }

    char line[64];
    int len = 0;
    while (1) {
        uint8_t c;
        if (uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(100)) <= 0) {
            continue;
        }
        if (c == '\r' || c == '\n') {
            putchar('\n');
            if (len > 0) {
                line[len] = '\0';
                handle_line(line);
                len = 0;
            }
        } else if (len < (int)sizeof(line) - 1) {
            line[len++] = c;
            putchar(c);   /* echo so the monitor shows what you type */
            fflush(stdout);
        }
    }
}
