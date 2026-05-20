/*
 * EdgeGuard-Sort M33 FreeRTOS Firmware — Entry Point
 *
 * Architecture per 统一深化方案:
 *   A55 Linux ←→ M33 FreeRTOS via RPMsg-Lite
 *   M33 runs 5 tasks: CommandRx, SortControl, Safety, StatusTx, VirtualIO
 *
 * Build:
 *   arm-none-eabi-gcc + FreeRTOS-Kernel + MCUXpresso SDK for i.MX93 M33
 *   or NXP MCUXpresso IDE project
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "sort_types.h"

/* ── RPMsg / Messaging ── */
/*
 * On i.MX93, A55 ↔ M33 communication uses RPMsg-Lite over the
 * Messaging Unit (MU). The A55 Linux remoteproc framework loads
 * this firmware and creates /dev/rpmsg0 for user-space access.
 *
 * For simulation/demo without A55 present, use loopback mode
 * (SIMULATION_MODE == 1).
 */
#define SIMULATION_MODE 1  /* 1 = self-test without A55 */

/* ── Global State ── */
static sort_command_t g_command;
static sort_status_t  g_status;
static SemaphoreHandle_t g_cmdMutex;
static QueueHandle_t     g_cmdQueue;       /* sort_command_t */
static QueueHandle_t     g_statusQueue;    /* sort_status_t */

/* ── Task Declarations ── */
static void CommandRxTask(void *pvParams);
static void SortControlTask(void *pvParams);
static void SafetyTask(void *pvParams);
static void StatusTxTask(void *pvParams);
static void VirtualIOTask(void *pvParams);

/* ── Simulation Helpers ── */
#if SIMULATION_MODE
static void sim_inject_test_commands(void);
#endif

/* Stack sizes (words) */
#define STACK_CMD_RX     512
#define STACK_SORT_CTRL  512
#define STACK_SAFETY     384
#define STACK_STATUS_TX  384
#define STACK_VIO        384

int main(void)
{
    /* Hardware init */
    /* BOARD_InitHardware(); — platform-specific, see MCUXpresso SDK */

    /* Create mutex & queues */
    g_cmdMutex = xSemaphoreCreateMutex();
    g_cmdQueue = xQueueCreate(4, sizeof(sort_command_t));
    g_statusQueue = xQueueCreate(8, sizeof(sort_status_t));

    /* Initialize status */
    memset(&g_status, 0, sizeof(g_status));
    g_status.state = SORT_IDLE;
    g_status.heartbeat_ok = 1;
    g_status.command_rx_ok = 1;
    g_status.sort_control_ok = 1;
    g_status.safety_task_ok = 1;
    g_status.status_tx_ok = 1;
    g_status.virtual_io_ok = 1;

    /* Create tasks — priority order: Safety > SortControl > CommandRx > StatusTx > VirtualIO */
    xTaskCreate(SafetyTask,       "Safety",       STACK_SAFETY,     NULL, 5, NULL);
    xTaskCreate(SortControlTask,  "SortCtrl",     STACK_SORT_CTRL,  NULL, 4, NULL);
    xTaskCreate(CommandRxTask,    "CmdRx",        STACK_CMD_RX,     NULL, 3, NULL);
    xTaskCreate(StatusTxTask,     "StatusTx",     STACK_STATUS_TX,  NULL, 2, NULL);
    xTaskCreate(VirtualIOTask,    "VirtualIO",    STACK_VIO,        NULL, 1, NULL);

#if SIMULATION_MODE
    /* Inject test commands for demo */
    sim_inject_test_commands();
#endif

    /* Start scheduler — never returns */
    vTaskStartScheduler();

    for (;;); /* Should never reach */
    return 0;
}

/* ── Simulation — inject test commands for standalone demo ── */
#if SIMULATION_MODE
#include <stdio.h>
static void sim_inject_test_commands(void)
{
    printf("[M33-SIM] Simulation mode — injecting test commands\n");

    /* Scenario 1: Normal sort to Zone A */
    sort_command_t cmd1 = {0};
    cmd1.frame_id = 1;
    cmd1.risk_level = LEVEL_0_NORMAL;
    cmd1.action = ACTION_PASS;
    cmd1.timeout_ms = 5000;
    strcpy(cmd1.package_id, "PKG001");
    strcpy(cmd1.target_zone, "A");
    xQueueSend(g_cmdQueue, &cmd1, portMAX_DELAY);
    printf("[M33-SIM] Injected: PKG001 → Zone A (Level 0)\n");

    /* Scenario 2: Review request */
    vTaskDelay(pdMS_TO_TICKS(3000));
    sort_command_t cmd2 = {0};
    cmd2.frame_id = 2;
    cmd2.risk_level = LEVEL_2_MEDIUM;
    cmd2.action = ACTION_REVIEW;
    cmd2.timeout_ms = 5000;
    strcpy(cmd2.package_id, "PKG002");
    strcpy(cmd2.target_zone, "REVIEW");
    xQueueSend(g_cmdQueue, &cmd2, portMAX_DELAY);
    printf("[M33-SIM] Injected: PKG002 → Review (Level 2)\n");

    /* Scenario 3: Emergency stop */
    vTaskDelay(pdMS_TO_TICKS(3000));
    sort_command_t cmd3 = {0};
    cmd3.frame_id = 3;
    cmd3.risk_level = LEVEL_4_CRITICAL;
    cmd3.action = ACTION_BLOCK;
    cmd3.timeout_ms = 5000;
    cmd3.emergency_stop = 1;
    strcpy(cmd3.package_id, "PKG003");
    strcpy(cmd3.target_zone, "BLOCK");
    xQueueSend(g_cmdQueue, &cmd3, portMAX_DELAY);
    printf("[M33-SIM] Injected: PKG003 → BLOCK (Level 4, Emergency)\n");

    printf("[M33-SIM] Test sequence complete\n");
}
#endif /* SIMULATION_MODE */
