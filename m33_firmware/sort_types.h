/*
 * EdgeGuard-Sort M33 Firmware — Shared Type Definitions
 * Maps A55 → M33 commands and M33 → A55 status per 统一深化方案 Table 6
 */

#ifndef SORT_TYPES_H
#define SORT_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* Risk level enumeration (Table 6) */
typedef enum {
    LEVEL_0_NORMAL   = 0,
    LEVEL_1_LOW      = 1,
    LEVEL_2_MEDIUM   = 2,
    LEVEL_3_HIGH     = 3,
    LEVEL_4_CRITICAL = 4
} risk_level_t;

/* Action from A55 DecisionEngine */
typedef enum {
    ACTION_PASS          = 0,
    ACTION_PASS_WITH_LOG = 1,
    ACTION_REVIEW        = 2,
    ACTION_BLOCK         = 3
} action_t;

/* M33 execution state (Table 5) */
typedef enum {
    SORT_IDLE          = 0,
    SORT_ROUTE_A       = 1,
    SORT_ROUTE_B       = 2,
    SORT_ROUTE_C       = 3,
    SORT_ROUTE_D       = 4,
    SORT_REVIEW        = 5,
    SORT_BLOCK         = 6,
    SAFETY_LOCK        = 7
} sort_state_t;

/* LED state */
typedef enum {
    LED_OFF    = 0,
    LED_GREEN  = 1,
    LED_YELLOW = 2,
    LED_RED    = 3
} led_state_t;

/* Buzzer state */
typedef enum {
    BUZZER_OFF        = 0,
    BUZZER_SLOW       = 1,  /* 1 Hz beep */
    BUZZER_FAST       = 2,  /* 4 Hz beep */
    BUZZER_CONTINUOUS = 3
} buzzer_state_t;

/* Motor state */
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_SLOW = 1,
    MOTOR_RUN  = 2
} motor_state_t;

/* Gate state */
typedef enum {
    GATE_CLOSED = 0,
    GATE_DIVERT = 1,
    GATE_OPEN   = 2
} gate_state_t;

/* ── A55 → M33 Command (via RPMsg) ── */
typedef struct __attribute__((packed)) {
    uint32_t    frame_id;
    uint32_t    risk_level;      /* risk_level_t */
    uint32_t    action;          /* action_t */
    uint32_t    timeout_ms;
    char        package_id[16];
    char        target_zone[8];  /* "A"/"B"/"C"/"D"/"REVIEW"/"BLOCK" */
    uint8_t     emergency_stop;  /* 1 = emergency */
    uint8_t     reserved[3];
} sort_command_t;

/* ── M33 → A55 Status (via RPMsg) ── */
typedef struct __attribute__((packed)) {
    uint32_t    frame_id;
    uint32_t    state;           /* sort_state_t */

    /* Actuators */
    uint32_t    led;             /* led_state_t */
    uint32_t    buzzer;          /* buzzer_state_t */
    uint32_t    motor;           /* motor_state_t */
    uint32_t    gate;            /* gate_state_t */
    char        chute[8];        /* target zone */

    /* Safety */
    uint32_t    heartbeat_count;
    uint32_t    timeout_count;
    uint8_t     heartbeat_ok;
    uint8_t     safety_locked;
    uint8_t     emergency_active;

    /* Task health */
    uint8_t     command_rx_ok;
    uint8_t     sort_control_ok;
    uint8_t     safety_task_ok;
    uint8_t     status_tx_ok;
    uint8_t     virtual_io_ok;

    uint8_t     reserved[3];
} sort_status_t;

#endif /* SORT_TYPES_H */
