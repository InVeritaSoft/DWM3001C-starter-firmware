/*! ----------------------------------------------------------------------------
 *  @file    orchestrator_tx.c
 *  @brief   Orchestrator TX example (Node A) - RS-485 controlled UWB transmitter
 *
 * @attention
 *
 * Copyright 2015 - 2021 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author Decawave
 */

#include "deca_probe_interface.h"
#include <deca_device_api.h>
#include <deca_spi.h>
#include <example_selection.h>
#include <port.h>
#include <shared_defines.h>
#include <shared_functions.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "app_uart.h"
#include "nrf_delay.h"
#include "app_timer.h"
#include "nrf_error.h"
#include "nrf_uart.h"
#include "app_util_platform.h"
#include "custom_board.h"
#include "nrf_gpio.h"
#include "boards.h"

#if defined(TEST_ORCHESTRATOR_TX)

extern void test_run_info(unsigned char *data);

/* Example application name */
#define APP_NAME "ORCHESTRATOR TX v1.0"

/* UART buffer size */
#define UART_BUFFER_SIZE 256
#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 256

/* UWB Configuration structure */
typedef struct {
    uint8_t channel;          // UWB channel: 5 or 9
    uint8_t data_rate;        // 0 = 850kbps, 1 = 6.8Mbps
    uint16_t preamble_len;    // e.g., 64, 128, 256
    uint8_t preamble_code;    // TX/RX preamble code
    uint8_t tx_power_idx;     // TX power index
    uint32_t pkt_rate_hz;     // TX packets per second
    uint16_t payload_len;     // Payload size in bytes
    uint8_t configured;       // Configuration flag
} uwb_config_t;

/* Test packet structure */
typedef struct {
    uint32_t seq;             // Sequence number
    uint32_t t_local;         // Local timestamp
    uint8_t payload[64];      // Payload data
} __attribute__((packed)) uwb_test_packet_t;

/* Statistics structure */
typedef struct {
    uint32_t total_sent;      // Total packets sent
    uint32_t last_error;      // Last error code
} tx_stats_t;

/* Global state */
static uwb_config_t g_config = {
    .channel = 5,
    .data_rate = 1,           // 6.8Mbps
    .preamble_len = 128,
    .preamble_code = 9,
    .tx_power_idx = 5,
    .pkt_rate_hz = 100,
    .payload_len = 64,
    .configured = 0
};

static dwt_config_t dwt_config = {
    5,                /* Channel number. */
    DWT_PLEN_128,     /* Preamble length. */
    DWT_PAC8,         /* Preamble acquisition chunk size. */
    9,                /* TX preamble code. */
    9,                /* RX preamble code. */
    1,                /* SFD type */
    DWT_BR_6M8,       /* Data rate. */
    DWT_PHRMODE_STD,  /* PHY header mode. */
    DWT_PHRRATE_STD,  /* PHY header rate. */
    (129 + 8 - 8),    /* SFD timeout. */
    DWT_STS_MODE_OFF, /* STS mode off. */
    DWT_STS_LEN_64,   /* STS length. */
    DWT_PDOA_M0       /* PDOA mode off. */
};

static tx_stats_t g_stats = {0};
static volatile uint8_t g_test_running = 0;
static volatile uint32_t g_seq_num = 0;
static uint8_t g_tx_buffer[FRAME_LEN_MAX];
static uwb_test_packet_t g_tx_packet;
static uint8_t g_timer_initialized = 0;

/* Timer for periodic TX */
APP_TIMER_DEF(m_tx_timer_id);

/* Forward declarations */
static void uart_event_handler(app_uart_evt_t *p_event);
static void parse_command(char *cmd);
static void send_response(const char *response);
static void send_test_char(char c);
static void configure_uwb(void);
static void tx_timer_handler(void *p_context);
static void send_packet(void);

/**
 * UART event handler
 */
static void uart_event_handler(app_uart_evt_t *p_event)
{
    static uint8_t rx_buffer[UART_BUFFER_SIZE];
    static uint16_t rx_index = 0;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
        {
            uint32_t err_code;
            uint8_t byte;
            err_code = app_uart_get(&byte);
            if (err_code == NRF_SUCCESS)
            {
                // Diagnostic: Echo received byte back to verify RX works
                // This helps debug if commands are being received
                send_test_char('R');  // 'R' for "Received"
                send_test_char('X');  // 'X' for "RX"
                send_test_char(':');
                send_test_char(byte >= 32 && byte < 127 ? byte : '?');
                send_test_char('\r');
                send_test_char('\n');
                
                if (byte == '\r' || byte == '\n')
                {
                    if (rx_index > 0)
                    {
                        rx_buffer[rx_index] = '\0';
                        
                        // ORANGE LED: RX - Command received - blink LED 1
                        bsp_board_led_on(1);
                        nrf_delay_ms(50);
                        bsp_board_led_off(1);
                        
                        // Send diagnostic before parsing
                        send_response("OK CMD_RECEIVED");
                        parse_command((char *)rx_buffer);
                        rx_index = 0;
                    }
                }
                else if (rx_index < (UART_BUFFER_SIZE - 1))
                {
                    rx_buffer[rx_index++] = byte;
                }
                else
                {
                    rx_index = 0; // Buffer overflow, reset
                }
            }
            break;
        }
        case APP_UART_COMMUNICATION_ERROR:
        case APP_UART_FIFO_ERROR:
            break;
        default:
            break;
    }
}

/**
 * Initialize UART for RS-485 communication
 */
static void uart_init(void)
{
    uint32_t err_code;
    uint32_t *p_err_code = &err_code;
    app_uart_comm_params_t comm_params =
    {
        .rx_pin_no = UART_0_RX_PIN,
        .tx_pin_no = UART_0_TX_PIN,
        .rts_pin_no = DW3000_RTS_PIN_NUM,
        .cts_pin_no = DW3000_CTS_PIN_NUM,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity = false,
        .baud_rate = 30801920  // 115200 baud
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUFFER_SIZE,
                       UART_TX_BUFFER_SIZE,
                       uart_event_handler,
                       APP_IRQ_PRIORITY_LOW,
                       p_err_code);
    
    if (err_code != NRF_SUCCESS)
    {
        test_run_info((unsigned char *)"UART INIT FAILED");
        // RED LED: Error state - intensive blinking for UART init failed
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        // Keep LED on after intensive blinking
        bsp_board_led_on(0);
        // Try to send error via UART anyway (might work if partially initialized)
        // Note: This might block if UART isn't ready, but we're debugging
        for (int i = 0; i < 10; i++) {
            if (app_uart_put('E') == NRF_SUCCESS) break;
            Sleep(10);
        }
    }
    else
    {
        // UART init succeeded - Blue LED will be turned on in orchestrator_tx()
        // Note: We send test chars in orchestrator_tx() function, not here
        // This function just initializes UART
    }
}

/**
 * Send response over UART (with timeout protection)
 */
static void send_response(const char *response)
{
    uint32_t len = strlen(response);
    uint32_t timeout;
    
    // GREEN LED: TX - Response being sent - turn on LED 2
    bsp_board_led_on(2);
    
    for (uint32_t i = 0; i < len; i++)
    {
        timeout = 1000;  // Max 1000 attempts
        while (app_uart_put(response[i]) != NRF_SUCCESS && timeout > 0)
        {
            timeout--;
            Sleep(1);
        }
        if (timeout == 0) break;  // Give up if can't send
    }
    
    timeout = 1000;
    while (app_uart_put('\r') != NRF_SUCCESS && timeout > 0)
    {
        timeout--;
        Sleep(1);
    }
    
    timeout = 1000;
    while (app_uart_put('\n') != NRF_SUCCESS && timeout > 0)
    {
        timeout--;
        Sleep(1);
    }
    
    // Turn off green LED after response sent
    nrf_delay_ms(10);
    bsp_board_led_off(2);
}

/**
 * Send a single test character (for debugging)
 */
static void send_test_char(char c)
{
    uint32_t timeout = 100;
    while (app_uart_put(c) != NRF_SUCCESS && timeout > 0)
    {
        timeout--;
        Sleep(1);
    }
}

/**
 * Parse SET_CONFIG command
 * Format: CFG ch=5 rate=6m8 pl=128 len=64 pwr=5 rate_hz=100
 */
static void parse_set_config(char *params)
{
    char *token = strtok(params, " ");
    uint8_t config_valid = 1;

    while (token != NULL)
    {
        if (strncmp(token, "ch=", 3) == 0)
        {
            g_config.channel = (uint8_t)atoi(token + 3);
            dwt_config.chan = g_config.channel;
        }
        else if (strncmp(token, "rate=", 5) == 0)
        {
            if (strcmp(token + 5, "6m8") == 0)
            {
                g_config.data_rate = 1;
                dwt_config.dataRate = DWT_BR_6M8;
            }
            else if (strcmp(token + 5, "850k") == 0)
            {
                g_config.data_rate = 0;
                dwt_config.dataRate = DWT_BR_850K;
            }
        }
        else if (strncmp(token, "pl=", 3) == 0)
        {
            uint16_t pl = (uint16_t)atoi(token + 3);
            g_config.preamble_len = pl;
            if (pl == 64) dwt_config.txPreambLength = DWT_PLEN_64;
            else if (pl == 128) dwt_config.txPreambLength = DWT_PLEN_128;
            else if (pl == 256) dwt_config.txPreambLength = DWT_PLEN_256;
            else if (pl == 512) dwt_config.txPreambLength = DWT_PLEN_512;
            else if (pl == 1024) dwt_config.txPreambLength = DWT_PLEN_1024;
        }
        else if (strncmp(token, "len=", 4) == 0)
        {
            g_config.payload_len = (uint16_t)atoi(token + 4);
        }
        else if (strncmp(token, "pwr=", 4) == 0)
        {
            g_config.tx_power_idx = (uint8_t)atoi(token + 4);
        }
        else if (strncmp(token, "rate_hz=", 8) == 0)
        {
            g_config.pkt_rate_hz = (uint32_t)atoi(token + 8);
        }

        token = strtok(NULL, " ");
    }

    if (config_valid)
    {
        configure_uwb();
        g_config.configured = 1;
        send_response("OK CONFIG");
    }
    else
    {
        send_response("ERR CONFIG");
    }
}

/**
 * Parse command string
 */
static void parse_command(char *cmd)
{
    // Convert to uppercase for comparison
    char cmd_upper[64];
    strncpy(cmd_upper, cmd, sizeof(cmd_upper) - 1);
    cmd_upper[sizeof(cmd_upper) - 1] = '\0';
    
    for (int i = 0; cmd_upper[i]; i++)
    {
        if (cmd_upper[i] >= 'a' && cmd_upper[i] <= 'z')
        {
            cmd_upper[i] = cmd_upper[i] - 'a' + 'A';
        }
    }

    // Handle commands (support both short and long forms)
    if (strcmp(cmd_upper, "PNG") == 0 || strcmp(cmd_upper, "PING") == 0)
    {
        // Diagnostic: Send immediate response (green LED will blink in send_response)
        send_response("OK");
    }
    else if (strncmp(cmd_upper, "NODE_TYPE", 9) == 0)
    {
        send_response("OK NODE_TYPE=TX");
    }
    else if (strncmp(cmd_upper, "CFG ", 4) == 0 || strncmp(cmd_upper, "SET_CONFIG ", 11) == 0)
    {
        char *params = strchr(cmd, ' ');
        if (params) params++; // Skip space
        else params = "";
        parse_set_config(params);
    }
    else if (strcmp(cmd_upper, "STRT") == 0 || strcmp(cmd_upper, "START_TEST") == 0)
    {
        if (!g_config.configured)
        {
            send_response("ERR NOT_CONFIGURED");
            return;
        }
        
        if (!g_timer_initialized)
        {
            send_response("ERR TIMER_NOT_INIT");
            return;
        }
        
        g_test_running = 1;
        g_seq_num = 0;
        g_stats.total_sent = 0;
        g_stats.last_error = 0;

        // Start timer for periodic TX
        uint32_t period_ms = 1000 / g_config.pkt_rate_hz;
        if (period_ms < 1) period_ms = 1;
        
        // Start the timer (already created during initialization)
        uint32_t err_code = app_timer_start(m_tx_timer_id, APP_TIMER_TICKS(period_ms), NULL);

        if (err_code == NRF_SUCCESS)
        {
            send_response("OK START");
        }
        else
        {
            send_response("ERR START_FAILED");
        }
    }
    else if (strcmp(cmd_upper, "STOP") == 0 || strcmp(cmd_upper, "STOP_TEST") == 0)
    {
        g_test_running = 0;
        app_timer_stop(m_tx_timer_id);
        send_response("OK STOP");
    }
    else if (strcmp(cmd_upper, "STAT") == 0 || strcmp(cmd_upper, "GET_STATS") == 0)
    {
        char stats_str[128];
        snprintf(stats_str, sizeof(stats_str), "OK STATS total_sent=%lu last_error=%lu",
                 (unsigned long)g_stats.total_sent, (unsigned long)g_stats.last_error);
        send_response(stats_str);
    }
    else if (strcmp(cmd_upper, "RST") == 0 || strcmp(cmd_upper, "RESET_STATS") == 0)
    {
        g_stats.total_sent = 0;
        g_stats.last_error = 0;
        g_seq_num = 0;
        send_response("OK");
    }
    else
    {
        send_response("ERR UNKNOWN_CMD");
    }
}

/**
 * Configure UWB radio
 */
static void configure_uwb(void)
{
    if (dwt_configure(&dwt_config))
    {
        test_run_info((unsigned char *)"CONFIG FAILED");
        return;
    }

    // Configure TX power (simplified - use default txconfig_options)
    extern dwt_txconfig_t txconfig_options;
    dwt_configuretxrf(&txconfig_options);
}

/**
 * TX timer handler - called periodically to send packets
 */
static void tx_timer_handler(void *p_context)
{
    if (g_test_running)
    {
        send_packet();
    }
}

/**
 * Send a test packet
 */
static void send_packet(void)
{
    uint32_t status_reg;

    // Prepare packet
    g_tx_packet.seq = g_seq_num++;
    g_tx_packet.t_local = dwt_readsystimestamphi32();
    
    // Fill payload with pattern
    for (int i = 0; i < g_config.payload_len && i < sizeof(g_tx_packet.payload); i++)
    {
        g_tx_packet.payload[i] = 0xAA;
    }

    // Copy to TX buffer
    // Calculate frame length: header (seq + t_local) + actual payload length
    uint16_t frame_len = sizeof(uint32_t) + sizeof(uint32_t) + g_config.payload_len + FCS_LEN;
    uint16_t data_len = sizeof(uint32_t) + sizeof(uint32_t) + g_config.payload_len;
    memcpy(g_tx_buffer, &g_tx_packet, data_len);

    // Write TX data
    dwt_writetxdata(data_len, g_tx_buffer, 0);
    dwt_writetxfctrl(frame_len, 0, 0);

    // Start transmission
    dwt_starttx(DWT_START_TX_IMMEDIATE);

    // Wait for TX complete
    waitforsysstatus(&status_reg, NULL, DWT_INT_TXFRS_BIT_MASK, 0);

    if (status_reg & DWT_INT_TXFRS_BIT_MASK)
    {
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
        g_stats.total_sent++;
        g_stats.last_error = 0;
    }
    else
    {
        g_stats.last_error = 1;
    }
}

/**
 * Application entry point
 */
int orchestrator_tx(void)
{
    uint32_t dev_id;
    uint32_t err_code;

    /* Display application name */
    test_run_info((unsigned char *)APP_NAME);
    
    /* LEDs are already initialized by bsp_board_init() in main.c */
    /* Initialize all LEDs to off */
    bsp_board_led_off(0);  // Red LED - Default/Error states
    bsp_board_led_off(1);  // Orange LED - RX (command received)
    bsp_board_led_off(2);  // Green LED - TX (response sent)
    bsp_board_led_off(3);  // Blue LED - UART works + Stand By state
    
    /* Red LED: Default state - blink to show firmware started */
    bsp_board_led_on(0);
    nrf_delay_ms(100);
    bsp_board_led_off(0);
    nrf_delay_ms(100);
    bsp_board_led_on(0);
    nrf_delay_ms(100);
    bsp_board_led_off(0);
    
    /* CRITICAL: Reset UART pins to default state before initialization */
    /* This removes any pull-up/pull-down resistors that might interfere */
    nrf_gpio_cfg_default(UART_0_RX_PIN);  // GPIO 15 (P0.15) - RX pin
    nrf_gpio_cfg_default(UART_0_TX_PIN);  // GPIO 19 (P0.19) - TX pin
    nrf_delay_ms(10);  // Small delay to ensure pin state is stable
    
    /* CRITICAL: Initialize UART FIRST, before anything else */
    /* This ensures UART works even if other init fails */
    uart_init();
    
    /* Blue LED: UART works - turn on to indicate UART initialized successfully */
    bsp_board_led_on(3);
    
    /* Wait for UART to be ready */
    Sleep(300);
    
    /* Send immediate test pattern - simple characters */
    /* This verifies UART TX works before any other code */
    for (int i = 0; i < 10; i++) {
        send_test_char('A' + (i % 26));  // Send A-Z pattern
        Sleep(20);
    }
    send_test_char('\r');
    send_test_char('\n');
    Sleep(50);
    
    /* Send startup message */
    send_response("OK STARTUP");
    Sleep(100);
    
    /* Send additional diagnostic info */
    send_response("OK FIRMWARE_RUNNING");
    Sleep(100);

    /* Initialize app timer module */
    err_code = app_timer_init();
    if (err_code != NRF_SUCCESS)
    {
        test_run_info((unsigned char *)"TIMER INIT FAILED");
        // RED LED: Error state - intensive blinking for timer init failed
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        // Keep LED on after intensive blinking
        bsp_board_led_on(0);
    }
    else
    {
        /* Create timer instance */
        err_code = app_timer_create(&m_tx_timer_id, APP_TIMER_MODE_REPEATED, tx_timer_handler);
        if (err_code == NRF_SUCCESS)
        {
            g_timer_initialized = 1;
        }
    }

    /* Configure SPI rate */
    port_set_dw_ic_spi_fastrate();

    /* Reset DW IC */
    reset_DWIC();
    Sleep(2);

    /* Probe for the correct device driver */
    dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);
    dev_id = dwt_readdevid();

    /* Wait for DW3000 to be ready with timeout */
    uint32_t timeout_count = 0;
    while (!dwt_checkidlerc() && timeout_count < 1000)
    {
        timeout_count++;
        Sleep(1);
    }
    
    if (timeout_count >= 1000)
    {
        send_response("ERR DW3000_TIMEOUT");
        test_run_info((unsigned char *)"DW3000 TIMEOUT");
        // RED LED: Error state - intensive blinking for DW3000 timeout
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        // Keep LED on after intensive blinking
        bsp_board_led_on(0);
    }

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
        send_response("ERR DW3000_INIT_FAILED");
        test_run_info((unsigned char *)"INIT FAILED");
        // RED LED: Error state - intensive blinking for DW3000 init failed
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        // Keep LED on after intensive blinking
        bsp_board_led_on(0);
        // Don't hang - continue so UART commands still work
    }
    else
    {
        send_response("OK DW3000_READY");
    }

    /* Enable LEDs */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure with default settings */
    configure_uwb();

    /* Send message that we reached main loop */
    Sleep(100);
    send_response("OK MAIN_LOOP");
    
    /* Blue LED: Stand By state - firmware running, ready for commands */
    /* Blue LED stays on (already on from UART init) to indicate stand by */
    
    /* Main loop - process UART commands */
    while (1)
    {
        // Command processing happens in UART interrupt handler
        Sleep(100);
    }
}

#endif

