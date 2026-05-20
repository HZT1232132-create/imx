/*
 * SortControlTask — Execute sort decisions per risk level (Table 6)
 *
 * Risk Level → Execution Mapping:
 *   Level 0: LED=GREEN  Buzzer=OFF    Motor=RUN   Gate=OPEN  Chute=TARGET
 *   Level 1: LED=GREEN  Buzzer=OFF    Motor=RUN   Gate=OPEN  Chute=TARGET  (log only)
 *   Level 2: LED=YELLOW Buzzer=SLOW   Motor=SLOW  Gate=DIVERT  Chute=REVIEW
 *   Level 3: LED=RED    Buzzer=FAST   Motor=STOP  Gate=CLOSED  Chute=REVIEW/BLOCK
 *   Level 4: LED=RED    Buzzer=CONT   Motor=STOP  Gate=CLOSED  Chute=BLOCK
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>
#include "sort_types.h"

extern QueueHandle_t g_cmdQueue;
extern QueueHandle_t g_statusQueue;
extern SemaphoreHandle_t g_cmdMutex;

static sort_status_t g_current_status;

/* Apply risk level → actuator mapping (Table 6) */
static void applyRiskLevel(const sort_command_t *cmd, sort_status_t *status)
{
    status->frame_id = cmd->frame_id;
    status->heartbeat_ok = 1;
    status->command_rx_ok = 1;
    status->sort_control_ok = 1;

    /* Emergency override */
    if (cmd->emergency_stop) {
        status->state       = SAFETY_LOCK;
        status->led         = LED_RED;
        status->buzzer      = BUZZER_CONTINUOUS;
        status->motor       = MOTOR_STOP;
        status->gate        = GATE_CLOSED;
        status->safety_locked = 1;
        status->emergency_active = 1;
        strcpy(status->chute, "BLOCK");
        return;
    }

    /* Clear emergency */
    status->emergency_active = 0;

    switch ((risk_level_t)cmd->risk_level) {
    case LEVEL_0_NORMAL:
        status->state       = (sort_state_t)(SORT_ROUTE_A + (cmd->target_zone[0] - 'A'));
        status->led         = LED_GREEN;
        status->buzzer      = BUZZER_OFF;
        status->motor       = MOTOR_RUN;
        status->gate        = GATE_OPEN;
        status->safety_locked = 0;
        strcpy(status->chute, cmd->target_zone);
        break;

    case LEVEL_1_LOW:
        status->state       = (sort_state_t)(SORT_ROUTE_A + (cmd->target_zone[0] - 'A'));
        status->led         = LED_GREEN;
        status->buzzer      = BUZZER_OFF;
        status->motor       = MOTOR_RUN;
        status->gate        = GATE_OPEN;
        status->safety_locked = 0;
        strcpy(status->chute, cmd->target_zone);
        break;

    case LEVEL_2_MEDIUM:
        status->state       = SORT_REVIEW;
        status->led         = LED_YELLOW;
        status->buzzer      = BUZZER_SLOW;
        status->motor       = MOTOR_SLOW;
        status->gate        = GATE_DIVERT;
        status->safety_locked = 0;
        strcpy(status->chute, "REVIEW");
        break;

    case LEVEL_3_HIGH:
        status->state       = SORT_REVIEW;
        status->led         = LED_RED;
        status->buzzer      = BUZZER_FAST;
        status->motor       = MOTOR_STOP;
        status->gate        = GATE_CLOSED;
        status->safety_locked = 0;
        strcpy(status->chute, "REVIEW");
        break;

    case LEVEL_4_CRITICAL:
        status->state       = SORT_BLOCK;
        status->led         = LED_RED;
        status->buzzer      = BUZZER_CONTINUOUS;
        status->motor       = MOTOR_STOP;
        status->gate        = GATE_CLOSED;
        status->safety_locked = 1;
        strcpy(status->chute, "BLOCK");
        break;

    default:
        status->state = SAFETY_LOCK;
        status->led = LED_RED;
        status->buzzer = BUZZER_CONTINUOUS;
        status->motor = MOTOR_STOP;
        status->gate = GATE_CLOSED;
        break;
    }
}

void SortControlTask(void *pvParams)
{
    sort_command_t cmd;

    /* Initialize status */
    memset(&g_current_status, 0, sizeof(g_current_status));
    g_current_status.state = SORT_IDLE;
    g_current_status.heartbeat_ok = 1;
    g_current_status.command_rx_ok = 1;
    g_current_status.sort_control_ok = 1;

    for (;;) {
        /* Wait for command from CommandRx */
        if (xQueueReceive(g_cmdQueue, &cmd, pdMS_TO_TICKS(1000)) == pdTRUE) {

            xSemaphoreTake(g_cmdMutex, portMAX_DELAY);
            applyRiskLevel(&cmd, &g_current_status);
            xSemaphoreGive(g_cmdMutex);

            /* Notify StatusTx that a new status is ready */
            xQueueSend(g_statusQueue, &g_current_status, 0);
        }

        /* Task health: increment heartbeat every successful loop */
        g_current_status.heartbeat_count++;
    }
}
