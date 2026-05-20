/*
 * SafetyTask — Heartbeat monitoring, emergency stop, A55 timeout detection
 *
 * Responsibilities (Table 5):
 *   - Monitor heartbeat from SortControlTask
 *   - Detect A55 command timeout
 *   - Emergency stop → SAFETY_LOCK all actuators
 *   - Report: heartbeat interval, timeout count, emergency flag
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include "sort_types.h"

extern sort_status_t g_current_status;
extern QueueHandle_t g_cmdQueue;
extern SemaphoreHandle_t g_cmdMutex;

#define HEARTBEAT_TIMEOUT_MS  3000  /* 3s no command → timeout */
#define MAX_TIMEOUT_COUNT     3     /* 3 consecutive timeouts → SAFETY_LOCK */

void SafetyTask(void *pvParams)
{
    TickType_t lastCmdTick = xTaskGetTickCount();
    uint32_t timeoutCount = 0;

    for (;;) {
        sort_command_t cmd;
        bool cmdReceived = (xQueuePeek(g_cmdQueue, &cmd, pdMS_TO_TICKS(500)) == pdTRUE);

        TickType_t now = xTaskGetTickCount();
        TickType_t elapsed = now - lastCmdTick;

        xSemaphoreTake(g_cmdMutex, portMAX_DELAY);

        if (cmdReceived) {
            /* Reset timeout on new command */
            lastCmdTick = now;
            timeoutCount = 0;
            g_current_status.timeout_count = 0;
            g_current_status.heartbeat_ok = 1;
            g_current_status.safety_task_ok = 1;

            /* Emergency stop input */
            if (cmd.emergency_stop) {
                g_current_status.state = SAFETY_LOCK;
                g_current_status.safety_locked = 1;
                g_current_status.emergency_active = 1;
                g_current_status.led = LED_RED;
                g_current_status.buzzer = BUZZER_CONTINUOUS;
                g_current_status.motor = MOTOR_STOP;
                g_current_status.gate = GATE_CLOSED;
                strcpy(g_current_status.chute, "BLOCK");
            }
        } else if (elapsed > pdMS_TO_TICKS(HEARTBEAT_TIMEOUT_MS)) {
            /* A55 timeout — no command received within timeout window */
            timeoutCount++;
            g_current_status.timeout_count = timeoutCount;

            if (timeoutCount >= MAX_TIMEOUT_COUNT) {
                /* 3 consecutive timeouts → SAFETY_LOCK */
                g_current_status.state = SAFETY_LOCK;
                g_current_status.safety_locked = 1;
                g_current_status.heartbeat_ok = 0;
                g_current_status.safety_task_ok = 0;
                g_current_status.led = LED_RED;
                g_current_status.buzzer = BUZZER_CONTINUOUS;
                g_current_status.motor = MOTOR_STOP;
                g_current_status.gate = GATE_CLOSED;
                strcpy(g_current_status.chute, "BLOCK");
            }
        }

        xSemaphoreGive(g_cmdMutex);

        /* Health — safety runs at ~10 Hz */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
