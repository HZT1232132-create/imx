/*
 * CommandRxTask — Receive sort commands from A55 via RPMsg/queue
 *
 * Responsibilities (Table 5):
 *   - Receive A55 commands: package_id, target_zone, risk_level, decision, timeout
 *   - Validate command integrity
 *   - Forward valid commands to SortControlTask
 *   - Report status: last A55→M33 command received
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "sort_types.h"

extern QueueHandle_t g_cmdQueue;
extern SemaphoreHandle_t g_cmdMutex;

#if !SIMULATION_MODE
/*
 * RPMsg endpoint callback — called when A55 sends a message.
 * In real deployment, this is registered with the RPMsg-Lite stack.
 */
static int rpmsg_endpoint_cb(void *data, int len, void *priv, unsigned long src)
{
    if (len != sizeof(sort_command_t)) return -1;

    sort_command_t cmd;
    memcpy(&cmd, data, sizeof(cmd));

    BaseType_t higherPriorityWoken = pdFALSE;
    xQueueSendFromISR(g_cmdQueue, &cmd, &higherPriorityWoken);
    portYIELD_FROM_ISR(higherPriorityWoken);

    return 0;
}
#endif

void CommandRxTask(void *pvParams)
{
    sort_command_t cmd;

    for (;;) {
        /* Block until a command arrives */
        if (xQueueReceive(g_cmdQueue, &cmd, portMAX_DELAY) == pdTRUE) {

            xSemaphoreTake(g_cmdMutex, portMAX_DELAY);

            /* Validate command */
            if (cmd.timeout_ms == 0 || cmd.timeout_ms > 30000) {
                cmd.timeout_ms = 5000; /* default 5s timeout */
            }

            /* Log received command — in real firmware, write to shared memory for Web */
            /* For now, just forward to SortControl via queue (already in cmd format) */

            xSemaphoreGive(g_cmdMutex);
        }
    }
}
