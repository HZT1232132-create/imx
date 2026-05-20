/*
 * VirtualIOTask — Maintain virtual IO state when no real hardware
 *
 * Responsibilities (Table 5):
 *   - Maintain virtual LED/buzzer/motor/chute state
 *   - Map to real GPIO/PWM when hardware present
 *   - When no hardware: maintain state in memory for StatusTx reporting
 *
 * GPIO Mapping (FRDM-i.MX93 P11 expansion header):
 *   LED Green  → gpiochip2 offset 0 (GPIO 576)
 *   LED Yellow → gpiochip2 offset 1 (GPIO 577)
 *   LED Red    → gpiochip2 offset 2 (GPIO 578)
 *   Buzzer     → gpiochip2 offset 3 (GPIO 579) (PWM-capable)
 *   Motor      → gpiochip2 offset 4 (GPIO 580) (PWM via timer)
 *   Gate servo → gpiochip2 offset 5 (GPIO 581) (PWM)
 */

#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include "sort_types.h"

extern sort_status_t g_current_status;

/* ── Hardware mode selection ── */
#define HW_MODE_VIRTUAL  0  /* No hardware — state in memory only */
#define HW_MODE_GPIO     1  /* libgpiod via sysfs on Linux M33 */
#define HW_MODE_MCU      2  /* Direct register access on bare-metal M33 */

#ifndef HW_MODE
#define HW_MODE HW_MODE_VIRTUAL
#endif

/* ── Virtual IO state ── */
typedef struct {
    uint32_t led;
    uint32_t buzzer;
    uint32_t motor;
    uint32_t gate;
    char     chute[8];
    uint32_t led_blink_counter;
} virtual_io_t;

static virtual_io_t g_vio;

#if HW_MODE == HW_MODE_GPIO
/* On Linux M33 user-space, use gpioset/gpioinfo */
static void gpio_set_led(led_state_t led)
{
    const char *val = "0";
    switch (led) {
    case LED_GREEN:  val = "1"; break;  /* gpioset gpiochip2 0=1 */
    case LED_YELLOW: val = "1"; break;  /* gpioset gpiochip2 1=1 */
    case LED_RED:    val = "1"; break;  /* gpioset gpiochip2 2=1 */
    default: break;
    }
    /* system() not ideal in RTOS — placeholder for actual GPIO HAL */
}
#endif

void VirtualIOTask(void *pvParams)
{
    memset(&g_vio, 0, sizeof(g_vio));
    strcpy(g_vio.chute, "IDLE");

    for (;;) {
        /* Read latest status */
        memcpy(&g_vio.led,    &g_current_status.led,    sizeof(uint32_t));
        memcpy(&g_vio.buzzer, &g_current_status.buzzer, sizeof(uint32_t));
        memcpy(&g_vio.motor,  &g_current_status.motor,  sizeof(uint32_t));
        memcpy(&g_vio.gate,   &g_current_status.gate,   sizeof(uint32_t));
        memcpy(g_vio.chute,   g_current_status.chute,   sizeof(g_vio.chute));

#if HW_MODE == HW_MODE_GPIO
        gpio_set_led(g_vio.led);
        /* ... buzzer PWM, motor PWM, gate servo ... */
#elif HW_MODE == HW_MODE_MCU
        /* Direct register writes for bare-metal M33 */
        /* GPIO_WritePinOutput(BOARD_LED_GREEN,  g_vio.led == LED_GREEN); */
        /* GPIO_WritePinOutput(BOARD_LED_YELLOW, g_vio.led == LED_YELLOW); */
        /* GPIO_WritePinOutput(BOARD_LED_RED,    g_vio.led == LED_RED); */
#else
        /* HW_MODE_VIRTUAL — state in memory, StatusTx reports it */
        g_vio.led_blink_counter++;
#endif

        /* Virtual IO status flag */
        g_current_status.virtual_io_ok = 1;

        /* Run at 50 Hz for responsive LED/Buzzer toggling */
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
