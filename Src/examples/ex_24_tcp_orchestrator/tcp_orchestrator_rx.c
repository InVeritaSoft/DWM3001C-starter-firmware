/*! ----------------------------------------------------------------------------
 *  @file    tcp_orchestrator_rx.c
 *  @brief   TCP-like Orchestrator RX example (Node B) - Reliable UWB receiver with ACK
 *           Based on orchestrator_rx_v2 with TCP-like features:
 *           - Sends ACK packets after receiving data packets
 *           - Handles duplicate packets
 *           - TCP-like statistics
 *
 * @attention
 *
 * Copyright 2015 - 2021 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author Decawave (Modified for TCP-like functionality)
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

#if defined(TEST_TCP_ORCHESTRATOR_RX)

extern void test_run_info(unsigned char *data);

/* Example application name */
#define APP_NAME "TCP ORCHESTRATOR RX v1.0"

/* UART buffer size */
#define UART_BUFFER_SIZE 256
#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 256

/* Packet flags */
#define TCP_FLAG_ACK   0x01
#define TCP_FLAG_SYN   0x02
#define TCP_FLAG_FIN   0x04
#define TCP_FLAG_RST   0x08

/* UWB Configuration structure */
typedef struct {
    uint8_t channel;          // UWB channel: 5 or 9
    uint8_t data_rate;        // 0 = 850kbps, 1 = 6.8Mbps
    uint16_t preamble_len;    // e.g., 64, 128, 256, 512, 1024
    uint8_t preamble_code;    // TX/RX preamble code
    uint32_t pkt_rate_hz;     // Expected TX packets per second
    uint16_t payload_len;     // Payload size in bytes
    uint8_t configured;       // Configuration flag
    
    // TCP-like parameters
    uint8_t tcp_mode;         // TCP mode enabled (1) or disabled (0)
    uint16_t window_size;     // Receiver window size
} uwb_config_t;

/* TCP-like packet structure */
typedef struct {
    uint32_t seq;             // Sequence number
    uint32_t ack_seq;         // Acknowledged sequence (0 if not ACK)
    uint8_t flags;            // TCP flags (ACK, SYN, FIN, RST)
    uint16_t window;          // Receiver window size
    uint32_t t_local;         // Local timestamp
    uint8_t payload[64];      // Payload data
} __attribute__((packed)) uwb_tcp_packet_t;

/* Enhanced statistics structure */
typedef struct {
    uint32_t total_rx;        // Total packets received successfully
    uint32_t lost_pkts;       // Lost packets (sequence gaps)
    uint32_t crc_err;         // CRC errors
    uint32_t phy_err;         // PHY header errors
    uint32_t rx_timeout;      // RX timeout errors
    uint32_t rx_overrun;      // RX buffer overrun errors
    uint32_t last_seq;        // Last received sequence number
    uint8_t seq_init;         // Sequence initialized flag
    
    // RF metrics (averaged)
    int64_t rssi_sum;
    int64_t fp_power_sum;
    int64_t pre_q_sum;
    int64_t fp_index_sum;
    uint32_t metric_count;
    
    // Min/Max tracking
    int32_t rssi_min;
    int32_t rssi_max;
    uint16_t pre_q_min;
    uint16_t pre_q_max;
    
    // TCP-like statistics
    uint32_t acks_sent;       // Number of ACK packets sent
    uint32_t duplicate_pkts;  // Duplicate packets received
} rx_stats_t;

/* Global state */
static uwb_config_t g_config = {
    .channel = 5,
    .data_rate = 1,           // 6.8Mbps
    .preamble_len = 128,
    .preamble_code = 9,
    .pkt_rate_hz = 100,
    .payload_len = 64,
    .configured = 0,
    .tcp_mode = 1,            // TCP mode enabled by default
    .window_size = 10
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
static uint8_t g_tx_buffer[FRAME_LEN_MAX];
static dwt_deviceentcnts_t g_event_cnts = {0};

/* Timer for ping-pong messages to Arduino (every 5 seconds) */
APP_TIMER_DEF(m_ping_timer_id);

/* Forward declarations */
static void uart_event_handler(app_uart_evt_t *p_event);
static uint32_t uart_init(void);
static void parse_command(char *cmd);
static void send_response(const char *response);
static void send_test_char(char c);
static void configure_uwb(void);
static void process_rx_packet(void);
static void ping_timer_handler(void *p_context);
static int32_t calculate_rssi_dbm(uint32_t channel_power);
static void update_rf_metrics(dwt_rxdiag_t *rx_diag);
static void send_ack_packet(uint32_t ack_seq);

/**
 * Calculate RSSI in dBm from channel power
 */
static int32_t calculate_rssi_dbm(uint32_t channel_power)
{
    if (channel_power == 0)
    {
        return -128;
    }
    
    uint32_t power_val = channel_power >> 16;
    int32_t rssi = (int32_t)(power_val) - 100;
    
    if (rssi < -128) rssi = -128;
    if (rssi > 0) rssi = 0;
    
    return rssi;
}

/**
 * Update RF metrics from diagnostics data
 */
static void update_rf_metrics(dwt_rxdiag_t *rx_diag)
{
    int32_t rssi = calculate_rssi_dbm(rx_diag->ipatovPower);
    uint16_t pre_q = rx_diag->ipatovAccumCount;
    uint16_t fp_index = rx_diag->ipatovFpIndex >> 6;
    
    g_stats.rssi_sum += rssi;
    g_stats.fp_power_sum += (int64_t)rx_diag->ipatovPower;
    g_stats.pre_q_sum += pre_q;
    g_stats.fp_index_sum += fp_index;
    g_stats.metric_count++;
    
    if (g_stats.metric_count == 1)
    {
        g_stats.rssi_min = rssi;
        g_stats.rssi_max = rssi;
        g_stats.pre_q_min = pre_q;
        g_stats.pre_q_max = pre_q;
    }
    else
    {
        if (rssi < g_stats.rssi_min) g_stats.rssi_min = rssi;
        if (rssi > g_stats.rssi_max) g_stats.rssi_max = rssi;
        if (pre_q < g_stats.pre_q_min) g_stats.pre_q_min = pre_q;
        if (pre_q > g_stats.pre_q_max) g_stats.pre_q_max = pre_q;
    }
}

/**
 * Send ACK packet to TX node
 */
static void send_ack_packet(uint32_t ack_seq)
{
    if (!g_config.tcp_mode) return;
    
    uwb_tcp_packet_t ack_packet;
    ack_packet.seq = 0;  // ACK packets don't have sequence numbers
    ack_packet.ack_seq = ack_seq;
    ack_packet.flags = TCP_FLAG_ACK;
    ack_packet.window = g_config.window_size;
    ack_packet.t_local = dwt_readsystimestamphi32();
    memset(ack_packet.payload, 0, sizeof(ack_packet.payload));
    
    uint16_t frame_len = sizeof(uwb_tcp_packet_t) + FCS_LEN;
    uint16_t data_len = sizeof(uwb_tcp_packet_t);
    memcpy(g_tx_buffer, &ack_packet, data_len);
    
    dwt_writetxdata(data_len, g_tx_buffer, 0);
    dwt_writetxfctrl(frame_len, 0, 0);
    
    dwt_forcetrxoff();
    dwt_starttx(DWT_START_TX_IMMEDIATE);
    
    uint32_t status_reg;
    waitforsysstatus(&status_reg, NULL, DWT_INT_TXFRS_BIT_MASK, 0);
    
    if (status_reg & DWT_INT_TXFRS_BIT_MASK)
    {
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
        g_stats.acks_sent++;
        
        char log_buf[64];
        snprintf(log_buf, sizeof(log_buf), "[TCP] ACK sent for seq=%lu", (unsigned long)ack_seq);
        test_run_info((unsigned char *)log_buf);
    }
}

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
                // DEBUG: Blink GREEN LED (LED_2, GPIO 22) on every byte received to confirm interrupt is working
                // NOTE: Blue LED (LED_3, GPIO 14) conflicts with UART RX pin, so using Green LED instead
                // This helps diagnose if interrupt handler is being called
                bsp_board_led_on(2);  // Green LED (GPIO 22) - shows interrupt is firing
                nrf_delay_ms(10);      // Short blink
                bsp_board_led_off(2);
                
                if (byte == '\r' || byte == '\n')
                {
                    if (rx_index > 0)
                    {
                        rx_buffer[rx_index] = '\0';
                        
                        // ORANGE LED: RX - Complete command received
                        bsp_board_led_on(1);
                        nrf_delay_ms(100);  // Longer blink for complete command
                        bsp_board_led_off(1);
                        
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
                    rx_index = 0;
                }
            }
            break;
        }
        case APP_UART_COMMUNICATION_ERROR:
        {
            NRF_UART0->ERRORSRC = 0xFFFFFFFF;
            rx_index = 0;
            break;
        }
        case APP_UART_FIFO_ERROR:
        {
            NRF_UART0->ERRORSRC = 0xFFFFFFFF;
            rx_index = 0;
            break;
        }
        default:
            break;
    }
}

/**
 * Initialize UART for RS-485 communication
 */
static uint32_t uart_init(void)
{
    uint32_t err_code;
    uint32_t *p_err_code = &err_code;
    
    // CRITICAL: Disable UART first to ensure clean state (matches orchestrator_v2)
    NRF_UART0->ENABLE = 0;  // Disable UART
    nrf_delay_ms(10);  // Wait for UART to fully disable
    
    // CRITICAL: Reset UART pins to default state IMMEDIATELY before APP_UART_FIFO_INIT
    // SDK may require pins to be unconfigured before UART takes control
    nrf_gpio_cfg_default(UART_0_RX_PIN);  // GPIO 15 (P0.15) - J10 Pin 10/15 (RXD0)
    nrf_gpio_cfg_default(UART_0_TX_PIN);  // GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX)
    nrf_delay_ms(20);  // Delay to ensure pin state is stable
    
    // CRITICAL: Configure RX pin as input with pullup AFTER reset but BEFORE UART init
    // This ensures proper pin state before UART takes control (from orchestrator_v2)
    nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_delay_ms(5);  // Small delay after pullup configuration
    
    app_uart_comm_params_t comm_params =
    {
        .rx_pin_no = UART_0_RX_PIN,
        .tx_pin_no = UART_0_TX_PIN,
        .rts_pin_no = 4,  // P0.4 (LED 1) - unused for flow control (matches orchestrator_v2)
        .cts_pin_no = 5,  // P0.5 (LED 2) - unused for flow control (matches orchestrator_v2)
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity = false,
        .baud_rate = 30801920  // 115200 baud (direct RS485 connection, no Arduino bridge)
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUFFER_SIZE,
                       UART_TX_BUFFER_SIZE,
                       uart_event_handler,
                       APP_IRQ_PRIORITY_LOW,
                       p_err_code);
    
    // CRITICAL: Explicitly enable UART, interrupts, and clear any errors (matches orchestrator_v2)
    if (err_code == NRF_SUCCESS) {
        // Clear any pending UART errors
        NRF_UART0->ERRORSRC = 0xFFFFFFFF;  // Clear all error sources
        
        // Explicitly enable UART (should already be enabled by APP_UART_FIFO_INIT, but ensure it)
        NRF_UART0->ENABLE = 4;  // 4 = UART_ENABLE_ENABLE_Enabled
        nrf_delay_ms(10);  // Small delay after explicit enable
        
        // CRITICAL: Enable UART RX interrupt AFTER UART is enabled
        // This is essential for the event handler to be called when data arrives
        NRF_UART0->INTENSET = (1UL << 2);  // UART_INTENSET_RXDRDY_Msk = bit 2 (RX data ready)
        nrf_delay_ms(10);  // Small delay after interrupt enable
    }
    
    nrf_delay_ms(100);
    
    if (err_code != NRF_SUCCESS)
    {
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "UART INIT FAILED: 0x%08X", err_code);
        test_run_info((unsigned char *)err_msg);
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        bsp_board_led_on(0);
    }
    else
    {
        test_run_info((unsigned char *)"UART INIT OK");
        // Don't send startup message - might block if UART isn't ready
    }
    
    return err_code;
}

/**
 * Send response over UART
 */
static void send_response(const char *response)
{
    uint32_t len = strlen(response);
    uint32_t timeout;
    uint32_t bytes_sent = 0;
    
    // Diagnostic: Log TX start
    char log_buf[64];
    snprintf(log_buf, sizeof(log_buf), "[DBG] TX start: '%s' (%lu bytes)", response, (unsigned long)len);
    test_run_info((unsigned char *)log_buf);
    
    // CRITICAL: Enable RS-485 transmit mode BEFORE sending data (if DE pin defined)
    #ifdef RS485_DE_PIN
    nrf_gpio_pin_set(RS485_DE_PIN);  // Set DE HIGH = transmit mode
    nrf_delay_us(50);  // Wait for transceiver to switch to transmit mode (typ. 10-30us)
    #endif
    
    // GREEN LED: TX - Response being sent (turn on at start)
    // NOTE: Green LED is also used for byte-by-byte RX indication, but TX takes priority
    bsp_board_led_on(2);
    
    // Send response string
    for (uint32_t i = 0; i < len; i++)
    {
        timeout = 1000;
        uint32_t uart_result;
        // Log first character for debugging
        if (i == 0) {
            snprintf(log_buf, sizeof(log_buf), "[DBG] UART put start: char='%c' (0x%02X)", response[i], (unsigned char)response[i]);
            test_run_info((unsigned char *)log_buf);
        }
        while ((uart_result = app_uart_put(response[i])) != NRF_SUCCESS && timeout > 0)
        {
            timeout--;
            Sleep(1);
        }
        if (uart_result != NRF_SUCCESS && timeout == 0) {
            snprintf(log_buf, sizeof(log_buf), "[DBG] UART put FAILED: char='%c' result=0x%08lX timeout", response[i], (unsigned long)uart_result);
            test_run_info((unsigned char *)log_buf);
        }
        if (timeout == 0) {
            // TX failed - blink red LED to indicate error
            bsp_board_led_off(2);
            bsp_board_led_on(0);  // Red LED = error
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            snprintf(log_buf, sizeof(log_buf), "[DBG] TX FAILED at byte %lu/%lu", (unsigned long)i, (unsigned long)len);
            test_run_info((unsigned char *)log_buf);
            return;  // Exit early on failure
        }
        bytes_sent++;
    }
    
    // Send carriage return
    timeout = 1000;
    while (app_uart_put('\r') != NRF_SUCCESS && timeout > 0)
    {
        timeout--;
        Sleep(1);
    }
    if (timeout > 0) {
        bytes_sent++;
    }
    
    // Send newline
    timeout = 1000;
    while (app_uart_put('\n') != NRF_SUCCESS && timeout > 0)
    {
        timeout--;
        Sleep(1);
    }
    if (timeout > 0) {
        bytes_sent++;
    }
    
    // Wait for UART transmission to complete
    // At 115200 baud: ~87us per byte, add margin to ensure all bytes are transmitted
    nrf_delay_ms(5);  // Increased delay to ensure transmission completes
    
    // CRITICAL: Disable RS-485 transmit mode AFTER sending data (if DE pin defined)
    #ifdef RS485_DE_PIN
    nrf_gpio_pin_clear(RS485_DE_PIN);  // Set DE LOW = receive mode
    nrf_delay_us(50);  // Wait for transceiver to switch to receive mode
    #endif
    
    // Keep LED on for 500ms so it's visible (for all command responses)
    nrf_delay_ms(500);
    bsp_board_led_off(2);
    
    // Diagnostic: Log TX completion
    snprintf(log_buf, sizeof(log_buf), "[DBG] TX complete: %lu bytes sent", (unsigned long)bytes_sent);
    test_run_info((unsigned char *)log_buf);
}

/**
 * Send a single test character
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
 * Format: CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100 tcp_mode=1 window_size=10
 * If params is empty or NULL, uses predefined default values
 */
static void parse_set_config(char *params)
{
    // If no parameters provided, use predefined defaults
    if (params == NULL || *params == '\0' || strlen(params) == 0)
    {
        // Default values are already set in g_config initialization
        // Just need to update dwt_config to match g_config
        dwt_config.chan = g_config.channel;
        dwt_config.dataRate = (g_config.data_rate == 1) ? DWT_BR_6M8 : DWT_BR_850K;
        
        // Set preamble length
        if (g_config.preamble_len == 64) dwt_config.txPreambLength = DWT_PLEN_64;
        else if (g_config.preamble_len == 128) dwt_config.txPreambLength = DWT_PLEN_128;
        else if (g_config.preamble_len == 256) dwt_config.txPreambLength = DWT_PLEN_256;
        else if (g_config.preamble_len == 512) dwt_config.txPreambLength = DWT_PLEN_512;
        else if (g_config.preamble_len == 1024) dwt_config.txPreambLength = DWT_PLEN_1024;
        
        configure_uwb();
        g_config.configured = 1;
        send_response("OK CONFIG");
        return;
    }
    
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
        else if (strncmp(token, "rate_hz=", 8) == 0)
        {
            g_config.pkt_rate_hz = (uint32_t)atoi(token + 8);
        }
        else if (strncmp(token, "tcp_mode=", 9) == 0)
        {
            g_config.tcp_mode = (uint8_t)atoi(token + 9);
        }
        else if (strncmp(token, "window_size=", 12) == 0)
        {
            g_config.window_size = (uint16_t)atoi(token + 12);
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
    // Trim leading and trailing whitespace
    while (*cmd == ' ' || *cmd == '\t' || *cmd == '\r' || *cmd == '\n')
    {
        cmd++;
    }
    
    char *end = cmd + strlen(cmd) - 1;
    while (end > cmd && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
    {
        *end = '\0';
        end--;
    }
    
    if (*cmd == '\0')
    {
        return; // Empty command
    }
    
    // Debug: log received command
    char log_buf[128];
    snprintf(log_buf, sizeof(log_buf), "[DBG] parse_command: '%s' (len=%lu)", cmd, (unsigned long)strlen(cmd));
    test_run_info((unsigned char *)log_buf);
    
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
    
    // Debug: log uppercase command
    snprintf(log_buf, sizeof(log_buf), "[DBG] cmd_upper: '%s'", cmd_upper);
    test_run_info((unsigned char *)log_buf);

    if (strcmp(cmd_upper, "PNG") == 0 || strcmp(cmd_upper, "PING") == 0)
    {
        test_run_info((unsigned char *)"[DBG] PNG received, sending OK response");
        send_response("OK");
        test_run_info((unsigned char *)"[DBG] PNG response sent");
        return;
    }
    else if (strncmp(cmd_upper, "NODE_TYPE", 9) == 0)
    {
        test_run_info((unsigned char *)"[DBG] NODE_TYPE command received");
        send_response("OK NODE_TYPE=RX_V2");
        test_run_info((unsigned char *)"[DBG] NODE_TYPE response sent");
        return;
    }
    else if (strcmp(cmd_upper, "CFG") == 0 || strcmp(cmd_upper, "CONFIG") == 0 || strcmp(cmd_upper, "SET_CONFIG") == 0)
    {
        // No parameters - use predefined defaults
        parse_set_config("");
    }
    else if (strncmp(cmd_upper, "CFG ", 4) == 0 || strncmp(cmd_upper, "SET_CONFIG ", 11) == 0)
    {
        // Has parameters - parse them
        char *params = strchr(cmd, ' ');
        if (params) params++;
        else params = "";
        parse_set_config(params);
    }
    else if (strcmp(cmd_upper, "STRT") == 0 || strcmp(cmd_upper, "START_TEST") == 0 || strcmp(cmd_upper, "START") == 0)
    {
        if (!g_config.configured)
        {
            send_response("ERR NOT_CONFIGURED");
            return;
        }
        
        g_test_running = 1;
        // Don't reset stats on START - let them accumulate (matches orchestrator_v2 behavior)
        // Only reset if explicitly requested via RESET_STATS command
        
        dwt_configeventcounters(1);
        dwt_configciadiag(1);
        
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
        
        send_response("OK START");
    }
    else if (strcmp(cmd_upper, "STOP") == 0 || strcmp(cmd_upper, "STOP_TEST") == 0)
    {
        g_test_running = 0;
        dwt_forcetrxoff();
        send_response("OK STOP");
    }
    else if (strcmp(cmd_upper, "STAT") == 0 || strcmp(cmd_upper, "GET_STATS") == 0 || strcmp(cmd_upper, "STATS") == 0)
    {
        dwt_readeventcounters(&g_event_cnts);
        
        int32_t rssi_avg = 0;
        int32_t fp_power_avg = 0;
        int32_t pre_q_avg = 0;
        int32_t fp_index_avg = 0;
        
        if (g_stats.metric_count > 0)
        {
            rssi_avg = (int32_t)(g_stats.rssi_sum / g_stats.metric_count);
            fp_power_avg = (int32_t)(g_stats.fp_power_sum / g_stats.metric_count);
            pre_q_avg = (int32_t)(g_stats.pre_q_sum / g_stats.metric_count);
            fp_index_avg = (int32_t)(g_stats.fp_index_sum / g_stats.metric_count);
        }
        
        char stats_str[512];
        if (g_config.tcp_mode)
        {
            snprintf(stats_str, sizeof(stats_str), 
                     "OK STATS rx=%lu lost=%lu crc_err=%lu phy_err=%lu timeout=%lu overrun=%lu "
                     "rssi_avg=%ld rssi_min=%ld rssi_max=%ld pre_q_avg=%ld pre_q_min=%u pre_q_max=%u "
                     "fp_index_avg=%ld evt_crc_good=%lu evt_crc_bad=%lu acks_sent=%lu dup_pkts=%lu",
                     (unsigned long)g_stats.total_rx,
                     (unsigned long)g_stats.lost_pkts,
                     (unsigned long)g_stats.crc_err,
                     (unsigned long)g_stats.phy_err,
                     (unsigned long)g_stats.rx_timeout,
                     (unsigned long)g_stats.rx_overrun,
                     (long)rssi_avg,
                     (long)g_stats.rssi_min,
                     (long)g_stats.rssi_max,
                     (long)pre_q_avg,
                     (unsigned int)g_stats.pre_q_min,
                     (unsigned int)g_stats.pre_q_max,
                     (long)fp_index_avg,
                     (unsigned long)g_event_cnts.CRCG,
                     (unsigned long)g_event_cnts.CRCB,
                     (unsigned long)g_stats.acks_sent,
                     (unsigned long)g_stats.duplicate_pkts);
        }
        else
        {
            snprintf(stats_str, sizeof(stats_str), 
                     "OK STATS rx=%lu lost=%lu crc_err=%lu phy_err=%lu timeout=%lu overrun=%lu "
                     "rssi_avg=%ld rssi_min=%ld rssi_max=%ld pre_q_avg=%ld pre_q_min=%u pre_q_max=%u "
                     "fp_index_avg=%ld evt_crc_good=%lu evt_crc_bad=%lu",
                     (unsigned long)g_stats.total_rx,
                     (unsigned long)g_stats.lost_pkts,
                     (unsigned long)g_stats.crc_err,
                     (unsigned long)g_stats.phy_err,
                     (unsigned long)g_stats.rx_timeout,
                     (unsigned long)g_stats.rx_overrun,
                     (long)rssi_avg,
                     (long)g_stats.rssi_min,
                     (long)g_stats.rssi_max,
                     (long)pre_q_avg,
                     (unsigned int)g_stats.pre_q_min,
                     (unsigned int)g_stats.pre_q_max,
                     (long)fp_index_avg,
                     (unsigned long)g_event_cnts.CRCG,
                     (unsigned long)g_event_cnts.CRCB);
        }
        send_response(stats_str);
    }
    else if (strcmp(cmd_upper, "RST") == 0 || strcmp(cmd_upper, "RESET_STATS") == 0)
    {
        memset(&g_stats, 0, sizeof(g_stats));
        memset(&g_event_cnts, 0, sizeof(g_event_cnts));
        dwt_configeventcounters(1);
        send_response("OK");
    }
    else
    {
        // Debug: log unknown command
        char log_buf[128];
        snprintf(log_buf, sizeof(log_buf), "[DBG] Unknown command: '%s'", cmd_upper);
        test_run_info((unsigned char *)log_buf);
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
    
    dwt_configeventcounters(1);
    dwt_configciadiag(1);
}

/**
 * Ping timer handler
 * DISABLED: Sending unsolicited messages causes issues with command parsing
 */
static void ping_timer_handler(void *p_context)
{
    // Disabled - was causing unsolicited "OK PING" messages
    // send_response("OK PING");
}

/**
 * Process received packet
 */
static void process_rx_packet(void)
{
    uint32_t status_reg;
    uint16_t frame_len;
    uwb_tcp_packet_t *rx_packet;

    status_reg = dwt_readsysstatuslo();
    
    if (!(status_reg & (DWT_INT_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_ERR)))
    {
        return;
    }

    if (status_reg & DWT_INT_RXFCG_BIT_MASK)
    {
        frame_len = dwt_getframelength();
        if (frame_len <= FRAME_LEN_MAX)
        {
            dwt_readrxdata(g_rx_buffer, frame_len - FCS_LEN, 0);
            dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);
            
            rx_packet = (uwb_tcp_packet_t *)g_rx_buffer;
            
            // Only process data packets (not ACK packets)
            if (!(rx_packet->flags & TCP_FLAG_ACK))
            {
                if (g_test_running)
                {
                    // Check for duplicate packet
                    if (g_stats.seq_init && rx_packet->seq <= g_stats.last_seq)
                    {
                        g_stats.duplicate_pkts++;
                        // Still send ACK for duplicate (helps TX node)
                        send_ack_packet(rx_packet->seq);
                        dwt_rxenable(DWT_START_RX_IMMEDIATE);
                        return;
                    }
                    
                    g_stats.total_rx++;
                    
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
                    
                    dwt_rxdiag_t rx_diag;
                    dwt_readdiagnostics(&rx_diag);
                    update_rf_metrics(&rx_diag);
                    
                    // Send ACK packet if TCP mode enabled
                    if (g_config.tcp_mode)
                    {
                        send_ack_packet(rx_packet->seq);
                    }
                    
                    dwt_rxenable(DWT_START_RX_IMMEDIATE);
                }
                else
                {
                    dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);
                }
            }
            else
            {
                // Received ACK packet (shouldn't happen on RX node, but handle it)
                dwt_rxenable(DWT_START_RX_IMMEDIATE);
            }
        }
    }
    else
    {
        if (g_test_running)
        {
            if (status_reg & DWT_INT_RXFCE_BIT_MASK)
            {
                g_stats.crc_err++;
            }
            if (status_reg & DWT_INT_RXPHE_BIT_MASK)
            {
                g_stats.phy_err++;
            }
            if (status_reg & DWT_INT_RXFTO_BIT_MASK)
            {
                g_stats.rx_timeout++;
            }
            if (status_reg & DWT_INT_RXOVRR_BIT_MASK)
            {
                g_stats.rx_overrun++;
            }
        }
        
        dwt_writesysstatuslo(SYS_STATUS_ALL_RX_ERR);
        
        if (g_test_running)
        {
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }
    }
}

/**
 * Application entry point
 */
int tcp_orchestrator_rx(void)
{
    uint32_t dev_id;
    uint32_t err_code;

    test_run_info((unsigned char *)APP_NAME);
    
    /* Initialize all LEDs to off */
    bsp_board_led_off(0);  // Red LED - Default/Error states
    bsp_board_led_off(1);  // Orange LED - RX (command received)
    bsp_board_led_off(2);  // Green LED - Byte-by-byte RX indicator (GPIO 22, doesn't conflict with UART)
    bsp_board_led_off(3);  // Blue LED - Not used (GPIO 14 conflicts with UART RX pin)
    
    bsp_board_led_on(0);
    nrf_delay_ms(100);
    bsp_board_led_off(0);
    nrf_delay_ms(100);
    bsp_board_led_on(0);
    nrf_delay_ms(100);
    bsp_board_led_off(0);
    
    // Diagnostic: Flash green LED (LED 2) to confirm we reach UART init point
    bsp_board_led_on(2);
    nrf_delay_ms(300);
    bsp_board_led_off(2);
    nrf_delay_ms(100);
    
    /* CRITICAL: Configure RX pin as input with pullup BEFORE UART initialization */
    /* This matches the demo firmware pattern (peripherals_init() does this) */
    /* The pin will be reset inside uart_init(), but this ensures initial state is correct */
    nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* CRITICAL: Initialize UART FIRST, before anything else */
    /* Pin reset is now done inside uart_init() immediately before APP_UART_FIFO_INIT */
    /* RX pin pullup is reconfigured inside uart_init() after the reset */
    uart_init();
    
    // Diagnostic: Flash red LED briefly to confirm UART init completed
    bsp_board_led_on(0);
    nrf_delay_ms(150);
    bsp_board_led_off(0);
    nrf_delay_ms(100);
    
    /* CRITICAL: Show LED pattern IMMEDIATELY after UART init (before any other operations) */
    /* LED pattern to indicate UART initialized:
     * - Orange LED (LED 1): UART initialized
     * - Green LED (LED 2): UART ready (blinks on each byte received)
     */
    // Make pattern very visible - 3 blinks with longer delays
    for (int i = 0; i < 3; i++) {
        bsp_board_led_on(1);  // Orange LED ON = UART initialized
        bsp_board_led_on(2);  // Green LED ON = UART ready
        nrf_delay_ms(800);  // Very long delay to make it visible
        bsp_board_led_off(1);
        bsp_board_led_off(2);
        nrf_delay_ms(300);
    }
    
    Sleep(300);
    test_run_info((unsigned char *)"OK STARTUP TCP");
    Sleep(100);
    
    err_code = app_timer_init();
    if (err_code != NRF_SUCCESS)
    {
        test_run_info((unsigned char *)"TIMER INIT FAILED");
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        // Don't leave red LED on - continue anyway
    }
    else
    {
        // Disabled ping timer - was causing unsolicited messages
        // err_code = app_timer_create(&m_ping_timer_id, APP_TIMER_MODE_REPEATED, ping_timer_handler);
        // if (err_code == NRF_SUCCESS)
        // {
        //     err_code = app_timer_start(m_ping_timer_id, APP_TIMER_TICKS(5000), NULL);
        //     if (err_code == NRF_SUCCESS)
        //     {
        //         test_run_info((unsigned char *)"PING timer started");
        //     }
        // }
    }

    // Don't call dwt_setleds() before DW3000 is initialized - it might crash
    // dwt_setleds() will be called after DW3000 init
    
    port_set_dw_ic_spi_fastrate();
    reset_DWIC();
    Sleep(2);

    dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf);
    dev_id = dwt_readdevid();

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
        // Disable DW3000 LEDs on timeout
        dwt_setleds(DWT_LEDS_DISABLE);
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        // Don't leave red LED on - continue anyway
    }

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
        send_response("ERR DW3000_INIT_FAILED");
        test_run_info((unsigned char *)"INIT FAILED");
        // Disable DW3000 LEDs on init failure
        dwt_setleds(DWT_LEDS_DISABLE);
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        // Don't leave red LED on - continue anyway
    }
    else
    {
        test_run_info((unsigned char *)"OK DW3000_READY");
        
        // Configure UWB before enabling LEDs
        configure_uwb();
        dwt_forcetrxoff();
        
        // Disable DW3000 LEDs completely to prevent infinite D13 flashing
        // D13 is the DW3000 clock pin and LED control causes continuous flashing
        dwt_setleds(DWT_LEDS_DISABLE);
    }

    Sleep(100);
    test_run_info((unsigned char *)"OK MAIN_LOOP");
    
    while (1)
    {
        if (g_test_running)
        {
            process_rx_packet();
            Sleep(1);
        }
        else
        {
            Sleep(100);
        }
    }
}

#endif
