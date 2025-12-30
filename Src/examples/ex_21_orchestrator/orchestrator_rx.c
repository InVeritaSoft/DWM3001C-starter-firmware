/*! ----------------------------------------------------------------------------
 *  @file    orchestrator_rx.c
 *  @brief   Orchestrator RX example (Node B) - RS-485 controlled UWB receiver
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
#include "nrf_error.h"
#include "nrf_uart.h"
#include "app_util_platform.h"
#include "custom_board.h"
#include "nrf_gpio.h"
#include "boards.h"

#if defined(TEST_ORCHESTRATOR_RX)

extern void test_run_info(unsigned char *data);

/* Example application name */
#define APP_NAME "ORCHESTRATOR RX v1.0"

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
    uint8_t tx_power_idx;     // TX power index (not used for RX)
    uint32_t pkt_rate_hz;     // Expected TX packets per second
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
    uint32_t total_rx;        // Total packets received
    uint32_t lost_pkts;       // Lost packets (sequence gaps)
    uint32_t crc_err;         // CRC errors
    uint32_t last_seq;        // Last received sequence number
    uint8_t seq_init;         // Sequence initialized flag
    
    int64_t rssi_sum;         // Sum of RSSI values
    int64_t snr_sum;          // Sum of SNR values
    int64_t pre_q_sum;        // Sum of preamble quality values
    uint32_t metric_count;    // Number of metrics collected
} rx_stats_t;

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

static rx_stats_t g_stats = {0};
static volatile uint8_t g_test_running = 0;
static uint8_t g_rx_buffer[FRAME_LEN_MAX];


/* Forward declarations */
static void uart_event_handler(app_uart_evt_t *p_event);
static void parse_command(char *cmd);
static void send_response(const char *response);
static void send_test_char(char c);
static void configure_uwb(void);
static void process_rx_packet(void);

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
        // UART init succeeded - Blue LED will be turned on in orchestrator_rx()
        // Note: We send test chars in orchestrator_rx() function, not here
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
        send_response("OK NODE_TYPE=RX");
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
        
        g_test_running = 1;
        memset(&g_stats, 0, sizeof(g_stats));
        
        // Ensure RX is enabled (may already be enabled)
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
        
        send_response("OK START");
    }
    else if (strcmp(cmd_upper, "STOP") == 0 || strcmp(cmd_upper, "STOP_TEST") == 0)
    {
        g_test_running = 0;
        // Note: RX remains enabled but process_rx_packet() won't process packets
        send_response("OK STOP");
    }
    else if (strcmp(cmd_upper, "STAT") == 0 || strcmp(cmd_upper, "GET_STATS") == 0)
    {
        int32_t rssi_avg = 0;
        int32_t snr_avg = 0;
        int32_t pre_q_avg = 0;
        
        if (g_stats.metric_count > 0)
        {
            rssi_avg = (int32_t)(g_stats.rssi_sum / g_stats.metric_count);
            snr_avg = (int32_t)(g_stats.snr_sum / g_stats.metric_count);
            pre_q_avg = (int32_t)(g_stats.pre_q_sum / g_stats.metric_count);
        }
        
        char stats_str[256];
        snprintf(stats_str, sizeof(stats_str), 
                 "OK STATS total_rx=%lu lost_pkts=%lu crc_err=%lu rssi_avg=%ld snr_avg=%ld pre_q_avg=%ld",
                 (unsigned long)g_stats.total_rx,
                 (unsigned long)g_stats.lost_pkts,
                 (unsigned long)g_stats.crc_err,
                 (long)rssi_avg,
                 (long)snr_avg,
                 (long)pre_q_avg);
        send_response(stats_str);
    }
    else if (strcmp(cmd_upper, "RST") == 0 || strcmp(cmd_upper, "RESET_STATS") == 0)
    {
        memset(&g_stats, 0, sizeof(g_stats));
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
}

/**
 * Process received packet (non-blocking)
 */
static void process_rx_packet(void)
{
    uint32_t status_reg;
    uint16_t frame_len;
    uwb_test_packet_t *rx_packet;

    // Check status register non-blocking
    status_reg = dwt_readsysstatuslo();
    
    // Check if we have a good RX frame or error
    if (!(status_reg & (DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_ERR)))
    {
        // No event yet, return without blocking
        return;
    }

    if (status_reg & DWT_INT_RXFCG_BIT_MASK)
    {
        // Good frame received
        frame_len = dwt_getframelength();
        if (frame_len <= FRAME_LEN_MAX)
        {
            dwt_readrxdata(g_rx_buffer, frame_len - FCS_LEN, 0);
            dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);
            
            rx_packet = (uwb_test_packet_t *)g_rx_buffer;
            
            // Update statistics
            g_stats.total_rx++;
            
            // Check sequence number
            if (!g_stats.seq_init)
            {
                g_stats.last_seq = rx_packet->seq;
                g_stats.seq_init = 1;
            }
            else
            {
                uint32_t expected_seq = g_stats.last_seq + 1;
                if (rx_packet->seq != expected_seq)
                {
                    if (rx_packet->seq > expected_seq)
                    {
                        g_stats.lost_pkts += (rx_packet->seq - expected_seq);
                    }
                }
                g_stats.last_seq = rx_packet->seq;
            }
            
            // Read RF metrics using diagnostics
            dwt_rxdiag_t rx_diag;
            dwt_readdiagnostics(&rx_diag);
            
            // Extract RSSI from diagnostics
            // RSSI can be calculated from the channel power (ipatovPower)
            // For simplicity, we'll use a simplified RSSI calculation
            // RSSI ≈ 10*log10(power) - A, where A is a calibration constant
            // Using ipatovPower as a proxy for signal strength
            // Note: This is a simplified calculation - actual RSSI requires calibration
            int32_t rssi_dbm = 0;  // Default value
            if (rx_diag.ipatovPower > 0)
            {
                // Simplified RSSI calculation (needs calibration for accurate values)
                // Using power value as an indicator
                rssi_dbm = (int32_t)(rx_diag.ipatovPower >> 16);  // Simplified extraction
            }
            
            // Preamble quality is represented by accumulator count
            // Higher accumulator count indicates better preamble quality
            uint16_t pre_q = rx_diag.ipatovAccumCount;
            
            // For SNR, we'll use a simplified calculation based on preamble quality
            // In practice, SNR calculation is more complex and may require diagnostics
            int32_t snr_est = (int32_t)pre_q;  // Simplified: use preamble quality as SNR estimate
            
            g_stats.rssi_sum += rssi_dbm;
            g_stats.snr_sum += snr_est;
            g_stats.pre_q_sum += pre_q;
            g_stats.metric_count++;
        }
        
        // Re-enable RX if test is running
        if (g_test_running)
        {
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }
    }
    else
    {
        // RX error
        g_stats.crc_err++;
        dwt_writesysstatuslo(SYS_STATUS_ALL_RX_ERR);
        
        // Re-enable RX if test is running
        if (g_test_running)
        {
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }
    }
}

/**
 * Application entry point
 */
int orchestrator_rx(void)
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
    nrf_gpio_cfg_default(UART_0_RX_PIN);  // GPIO 15 (P0.15) - RX pin (Pin 10 on J10)
    nrf_gpio_cfg_default(UART_0_TX_PIN);  // GPIO 14 (P0.14) - TX pin (Pin 8 on J10)
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

    /* Enable RX initially (will be controlled by START/STOP commands) */
    dwt_rxenable(DWT_START_RX_IMMEDIATE);

    /* Send message that we reached main loop */
    Sleep(100);
    send_response("OK MAIN_LOOP");
    
    /* Blue LED: Stand By state - firmware running, ready for commands */
    /* Blue LED stays on (already on from UART init) to indicate stand by */
    
    /* Main loop - process UART commands and RX packets */
    while (1)
    {
        if (g_test_running)
        {
            process_rx_packet();
        }
        else
        {
            Sleep(100);
        }
    }
}

#endif

