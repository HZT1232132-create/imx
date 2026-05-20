/*
 * StatusTxTask — Send M33 execution status back to A55 / Web
 *
 * Responsibilities (Table 5):
 *   - Periodically send M33 status JSON to A55 via RPMsg
 *   - Report all task health flags
 *   - In simulation mode: print status to console
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include "sort_types.h"

extern sort_status_t g_current_status;
extern QueueHandle_t g_statusQueue;

#if SIMULATION_MODE
#include <stdio.h>

static const char* state_names[] = {
    "IDLE", "SORT_ROUTE_A", "SORT_ROUTE_B", "SORT_ROUTE_C", "SORT_ROUTE_D",
    "SORT_REVIEW", "SORT_BLOCK", "SAFETY_LOCK"
};
static const char* led_names[]  = { "off", "green", "yellow", "red" };
static const char* buzzer_names[] = { "off", "slow", "fast", "continuous" };
static const char* motor_names[]  = { "stop", "slow", "run" };
static const char* gate_names[]   = { "closed", "divert", "open" };
#endif

void StatusTxTask(void *pvParams)
{
    sort_status_t status;

    for (;;) {
        /* Wait for updated status from SortControl */
        if (xQueueReceive(g_statusQueue, &status, pdMS_TO_TICKS(500)) == pdTRUE) {
            memcpy(&g_current_status, &status, sizeof(sort_status_t));
        }

#if SIMULATION_MODE
        /* Console output for demo */
        printf("[M33-STATUS] frame=%u state=%s led=%s buz=%s motor=%s gate=%s chute=%s "
               "hb=%u hb_ok=%d safe=%d tasks=%d%d%d%d%d\n",
               (unsigned)status.frame_id,
               state_names[status.state < 8 ? status.state : 0],
               led_names[status.led < 4 ? status.led : 0],
               buzzer_names[status.buzzer < 4 ? status.buzzer : 0],
               motor_names[status.motor < 3 ? status.motor : 0],
               gate_names[status.gate < 3 ? status.gate : 0],
               status.chute,
               (unsigned)status.heartbeat_count,
               status.heartbeat_ok,
               status.safety_locked,
               status.command_rx_ok,
               status.sort_control_ok,
               status.safety_task_ok,
               status.status_tx_ok,
               status.virtual_io_ok);
#else
        /* Real deployment: send via RPMsg to A55 */
        /* rpmsg_send(status_endpoint, &status, sizeof(status)); */
#endif

        /* Report every 500ms */
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
