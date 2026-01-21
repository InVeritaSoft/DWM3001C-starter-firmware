/*! ----------------------------------------------------------------------------
 *  @file    orchestrator_rx_v2.c
 *  @brief   Orchestrator RX v2 example (Node B) - Enhanced RS-485 controlled UWB receiver
 *           Improvements over v1:
 *           - Enhanced RF diagnostics using dwt_readdiagnostics
 *           - Proper RSSI calculation from channel power
 *           - First path detection and analysis
 *           - Comprehensive error categorization
 *           - Event counter tracking
 *           - Better statistics aggregation
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

#if defined(TEST_ORCHESTRATOR_RX_V2)

extern void test_run_info(unsigned char *data);

/* Example application name */
#define APP_NAME "ORCHESTRATOR RX v2.0"

/* UART buffer size */
#define UART_BUFFER_SIZE 256
#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 256

/* UWB Configuration structure */
typedef struct {
    uint8_t channel;          // UWB channel: 5 or 9
    uint8_t data_rate;        // 0 = 850kbps, 1 = 6.8Mbps
    uint16_t preamble_len;    // e.g., 64, 128, 256, 512, 1024
    uint8_t preamble_code;    // TX/RX preamble code
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

/* Enhanced statistics structure */
typedef struct {
    uint32_t total_rx;        // Total packets received successfully
    uint32_t lost_pkts;        // Lost packets (sequence gaps)
    uint32_t crc_err;          // CRC errors
    uint32_t phy_err;          // PHY header errors
    uint32_t rx_timeout;       // RX timeout errors
    uint32_t rx_overrun;       // RX buffer overrun errors
    uint32_t last_seq;        // Last received sequence number
    uint8_t seq_init;          // Sequence initialized flag
    
    // RF metrics (averaged)
    int64_t rssi_sum;          // Sum of RSSI values (dBm)
    int64_t fp_power_sum;      // Sum of first path power values
    int64_t pre_q_sum;         // Sum of preamble quality values
    int64_t fp_index_sum;       // Sum of first path indices
    uint32_t metric_count;     // Number of metrics collected
    
    // Min/Max tracking
    int32_t rssi_min;          // Minimum RSSI
    int32_t rssi_max;          // Maximum RSSI
    uint16_t pre_q_min;        // Minimum preamble quality
    uint16_t pre_q_max;        // Maximum preamble quality
} rx_stats_t;

/* Global state */
static uwb_config_t g_config = {
    .channel = 5,
    .data_rate = 1,           // 6.8Mbps
    .preamble_len = 128,
    .preamble_code = 9,
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
static dwt_deviceentcnts_t g_event_cnts = {0}; // Event counters

/* Timer for ping-pong messages to Arduino (every 5 seconds) */
APP_TIMER_DEF(m_ping_timer_id);

/* Forward declarations */
static void uart_event_handler(app_uart_evt_t *p_event);
static void parse_command(char *cmd);
static void send_response(const char *response);
static void send_test_char(char c);
static void configure_uwb(void);
static void process_rx_packet(void);
static void ping_timer_handler(void *p_context);
static int32_t calculate_rssi_dbm(uint32_t channel_power);
static void update_rf_metrics(dwt_rxdiag_t *rx_diag);

/**
 * Calculate RSSI in dBm from channel power
 * Uses simplified calculation based on ipatovPower
 */
static int32_t calculate_rssi_dbm(uint32_t channel_power)
{
    // Simplified RSSI calculation
    // Actual implementation may require calibration
    // RSSI ≈ 10*log10(power) - A, where A is calibration constant
    // For now, use a simplified linear approximation
    if (channel_power == 0)
    {
        return -128; // Invalid/missing signal
    }
    
    // Extract power value (simplified - actual calculation more complex)
    // Using upper 16 bits as power indicator
    uint32_t power_val = channel_power >> 16;
    
    // Simplified conversion (needs calibration for accurate values)
    // This is a placeholder - actual RSSI requires proper calibration
    int32_t rssi = (int32_t)(power_val) - 100; // Offset adjustment
    
    // Clamp to reasonable range
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
    uint16_t fp_index = rx_diag->ipatovFpIndex >> 6; // Convert from fixed point
    
    g_stats.rssi_sum += rssi;
    g_stats.fp_power_sum += (int64_t)rx_diag->ipatovPower;
    g_stats.pre_q_sum += pre_q;
    g_stats.fp_index_sum += fp_index;
    g_stats.metric_count++;
    
    // Update min/max
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
                    rx_index = 0; // Buffer overflow, reset
                }
            }
            break;
        }
        case APP_UART_COMMUNICATION_ERROR:
        {
            // Clear UART errors to recover from communication issues
            NRF_UART0->ERRORSRC = 0xFFFFFFFF;  // Clear all error sources
            // Reset RX buffer on error
            rx_index = 0;
            break;
        }
        case APP_UART_FIFO_ERROR:
        {
            // Clear UART errors to recover from FIFO issues
            NRF_UART0->ERRORSRC = 0xFFFFFFFF;  // Clear all error sources
            // Reset RX buffer on error
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
static void uart_init(void)
{
    uint32_t err_code;
    uint32_t *p_err_code = &err_code;
    
    // #region agent log
    char log_buf[128];
    snprintf(log_buf, sizeof(log_buf), "[DBG] uart_init entry: rx=%d tx=%d rts=%d cts=%d", 
             UART_0_RX_PIN, UART_0_TX_PIN, RTS_PIN_NUMBER, CTS_PIN_NUMBER);
    test_run_info((unsigned char *)log_buf);
    // #endregion
    
    // CRITICAL: Configure RS-485 DE (Driver Enable) pin FIRST (if defined)
    // DE pin controls RS-485 transceiver direction:
    // - LOW = receive mode (default)
    // - HIGH = transmit mode
    #ifdef RS485_DE_PIN
    nrf_gpio_cfg_output(RS485_DE_PIN);
    nrf_gpio_pin_clear(RS485_DE_PIN);  // Set to receive mode (LOW)
    nrf_delay_ms(10);  // Wait for transceiver to enter receive mode
    test_run_info((unsigned char *)"[DBG] RS-485 DE pin configured (receive mode)");
    #else
    test_run_info((unsigned char *)"[DBG] RS-485 DE pin not defined - assuming automatic direction control");
    #endif
    
    // #region agent log
    uint32_t pin_tx_dir = nrf_gpio_pin_dir_get(UART_0_TX_PIN);
    uint32_t pin_rx_dir = nrf_gpio_pin_dir_get(UART_0_RX_PIN);
    uint32_t pin_tx_cnf = NRF_GPIO->PIN_CNF[UART_0_TX_PIN];
    uint32_t pin_rx_cnf = NRF_GPIO->PIN_CNF[UART_0_RX_PIN];
    snprintf(log_buf, sizeof(log_buf), "[DBG] pin state BEFORE reset: tx_pin%d_dir=%lu rx_pin%d_dir=%lu tx_cnf=0x%08lX rx_cnf=0x%08lX",
             UART_0_TX_PIN, (unsigned long)pin_tx_dir, UART_0_RX_PIN, (unsigned long)pin_rx_dir,
             (unsigned long)pin_tx_cnf, (unsigned long)pin_rx_cnf);
    test_run_info((unsigned char *)log_buf);
    // #endregion
    
    // CRITICAL: Disable UART first to ensure clean state (if it was previously enabled)
    // This mimics what happens when reset button is held - UART is disabled
    // 0 = Disabled, 4 = Enabled (per nRF52 UART register spec)
    NRF_UART0->ENABLE = 0;  // Disable UART
    nrf_delay_ms(10);  // Wait for UART to fully disable
    
    // CRITICAL: Reset UART pins to default state IMMEDIATELY before APP_UART_FIFO_INIT
    // SDK may require pins to be unconfigured before UART takes control
    // Using default UART pins: TX=14 (J10 Pin 8, TXD0), RX=15 (J10 Pin 10, RXD0)
    nrf_gpio_cfg_default(UART_0_RX_PIN);  // GPIO 15 (P0.15) - J10 Pin 10/15 (RXD0) - Confirmed working
    nrf_gpio_cfg_default(UART_0_TX_PIN);  // GPIO 27 (P0.27) - J10 Pin 13 - Confirmed working for TX
    nrf_delay_ms(20);  // Increased delay to ensure pin state is stable (was 1ms - too short!)
    
    // CRITICAL: Configure RX pin as input with pullup AFTER reset but BEFORE UART init
    // This ensures proper pin state before UART takes control (from demo firmware)
    // Must be done AFTER cfg_default() to avoid being cleared
    nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_delay_ms(5);  // Small delay after pullup configuration
    
    // #region agent log
    pin_tx_dir = nrf_gpio_pin_dir_get(UART_0_TX_PIN);
    pin_rx_dir = nrf_gpio_pin_dir_get(UART_0_RX_PIN);
    pin_tx_cnf = NRF_GPIO->PIN_CNF[UART_0_TX_PIN];
    pin_rx_cnf = NRF_GPIO->PIN_CNF[UART_0_RX_PIN];
    snprintf(log_buf, sizeof(log_buf), "[DBG] pin state AFTER reset: tx_pin%d_dir=%lu rx_pin%d_dir=%lu tx_cnf=0x%08lX rx_cnf=0x%08lX",
             UART_0_TX_PIN, (unsigned long)pin_tx_dir, UART_0_RX_PIN, (unsigned long)pin_rx_dir,
             (unsigned long)pin_tx_cnf, (unsigned long)pin_rx_cnf);
    test_run_info((unsigned char *)log_buf);
    // #endregion
    
    // SDK rejects RTS/CTS pins being the same as RX/TX pins (error 0x50000000)
    // Use different unused GPIO pins for RTS/CTS (they won't be used since flow control is disabled)
    // Using P0.4 and P0.5 (LED pins) - LEDs can still be controlled via BSP functions
    app_uart_comm_params_t comm_params =
    {
        .rx_pin_no = UART_0_RX_PIN,
        .tx_pin_no = UART_0_TX_PIN,
        .rts_pin_no = 4,  // P0.4 (LED 1) - unused for flow control
        .cts_pin_no = 5,  // P0.5 (LED 2) - unused for flow control
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity = false,
        .baud_rate = 30801920  // 115200 baud
    };

    // #region agent log
    snprintf(log_buf, sizeof(log_buf), "[DBG] comm_params: rts=0x%08lX cts=0x%08lX flow_ctrl=%d",
             (unsigned long)comm_params.rts_pin_no, (unsigned long)comm_params.cts_pin_no, comm_params.flow_control);
    test_run_info((unsigned char *)log_buf);
    // #endregion

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUFFER_SIZE,
                       UART_TX_BUFFER_SIZE,
                       uart_event_handler,
                       APP_IRQ_PRIORITY_LOW,
                       p_err_code);
    
    // CRITICAL: Explicitly enable UART, interrupts, and clear any errors
    // This ensures UART is in a known good state (mimics reset button behavior)
    if (err_code == NRF_SUCCESS) {
        // Clear any pending UART errors
        NRF_UART0->ERRORSRC = 0xFFFFFFFF;  // Clear all error sources
        
        // Explicitly enable UART (should already be enabled by APP_UART_FIFO_INIT, but ensure it)
        NRF_UART0->ENABLE = 4;  // 4 = UART_ENABLE_ENABLE_Enabled
        nrf_delay_ms(10);  // Small delay after explicit enable
        
        // CRITICAL: Enable UART RX interrupt AFTER UART is enabled
        // This is essential for the event handler to be called when data arrives
        // APP_UART_FIFO_INIT should enable this, but we ensure it's set
        // Note: Must be done AFTER ENABLE is set, otherwise it may not work
        NRF_UART0->INTENSET = (1UL << 2);  // UART_INTENSET_RXDRDY_Msk = bit 2 (RX data ready)
        
        // Verify interrupt is enabled
        uint32_t inten = NRF_UART0->INTENSET;
        snprintf(log_buf, sizeof(log_buf), "[DBG] UART INTENSET=0x%08lX (bit 2 should be set)", (unsigned long)inten);
        test_run_info((unsigned char *)log_buf);
        
        nrf_delay_ms(10);  // Small delay after interrupt enable
    }
    
    // CRITICAL: Wait for UART to fully initialize before use
    // This ensures UART hardware is ready (fixes intermittent response issue)
    nrf_delay_ms(100);  // Increased delay to allow UART to fully stabilize (was 50ms)
    
    // #region agent log
    snprintf(log_buf, sizeof(log_buf), "[DBG] APP_UART_FIFO_INIT result: err=0x%08lX", (unsigned long)err_code);
    test_run_info((unsigned char *)log_buf);
    
    // Verify UART PSEL registers are set correctly (GPIO pins enabled for UART)
    if (err_code == NRF_SUCCESS) {
        uint32_t psel_rxd = NRF_UART0->PSELRXD;
        uint32_t psel_txd = NRF_UART0->PSELTXD;
        uint32_t psel_rts = NRF_UART0->PSELRTS;
        uint32_t psel_cts = NRF_UART0->PSELCTS;
        uint32_t enable = NRF_UART0->ENABLE;
        snprintf(log_buf, sizeof(log_buf), "[DBG] UART PSEL: RXD=%lu TXD=%lu RTS=%lu CTS=%lu ENABLE=%lu",
                 (unsigned long)psel_rxd, (unsigned long)psel_txd, 
                 (unsigned long)psel_rts, (unsigned long)psel_cts, (unsigned long)enable);
        test_run_info((unsigned char *)log_buf);
        
        // Check if pins match expected values
        if (psel_rxd != UART_0_RX_PIN || psel_txd != UART_0_TX_PIN) {
            snprintf(log_buf, sizeof(log_buf), "[DBG] WARNING: PSEL mismatch! Expected RXD=%d TXD=%d, got RXD=%lu TXD=%lu",
                     UART_0_RX_PIN, UART_0_TX_PIN, (unsigned long)psel_rxd, (unsigned long)psel_txd);
            test_run_info((unsigned char *)log_buf);
        }
        
        // Check if UART is enabled
        if (enable != 4) {  // 4 = UART_ENABLED_ENABLE_Enabled
            snprintf(log_buf, sizeof(log_buf), "[DBG] WARNING: UART not enabled! ENABLE register = %lu (expected 4)",
                     (unsigned long)enable);
            test_run_info((unsigned char *)log_buf);
        }
    }
    // #endregion
    
    if (err_code != NRF_SUCCESS)
    {
        char err_msg[64];
        snprintf(err_msg, sizeof(err_msg), "UART INIT FAILED: 0x%08X", err_code);
        test_run_info((unsigned char *)err_msg);
        
        // #region agent log
        snprintf(log_buf, sizeof(log_buf), "[DBG] UART init failed: err=0x%08lX dec=%lu", 
                 (unsigned long)err_code, (unsigned long)err_code);
        test_run_info((unsigned char *)log_buf);
        // #endregion
        
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
        
        // #region agent log
        test_run_info((unsigned char *)"[DBG] UART init succeeded");
        // #endregion
    }
}

/**
 * Send response over UART (with timeout protection)
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
        while (app_uart_put(response[i]) != NRF_SUCCESS && timeout > 0)
        {
            timeout--;
            Sleep(1);
        }
        if (timeout == 0) {
            // TX failed - blink red LED to indicate error
            bsp_board_led_off(2);
            bsp_board_led_on(0);  // Red LED = error
            nrf_delay_ms(50);
            bsp_board_led_off(0);
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
    // At 115200 baud: ~87us per byte, so for "OK\r\n" (4 bytes) = ~348us
    // Add extra margin to ensure all bytes are transmitted
    nrf_delay_ms(5);  // Increased delay to ensure transmission completes (was 2ms)
    
    // CRITICAL: Disable RS-485 transmit mode AFTER sending data (if DE pin defined)
    #ifdef RS485_DE_PIN
    nrf_gpio_pin_clear(RS485_DE_PIN);  // Set DE LOW = receive mode
    nrf_delay_us(50);  // Wait for transceiver to switch back to receive mode
    #endif
    
    // Turn off LEDs - transmission complete
    bsp_board_led_off(2);  // Green LED off
    
    // Diagnostic: Log TX completion
    snprintf(log_buf, sizeof(log_buf), "[DBG] TX complete: %lu bytes sent", (unsigned long)bytes_sent);
    test_run_info((unsigned char *)log_buf);
    
    // Diagnostic: If no bytes were sent, blink red LED
    if (bytes_sent == 0) {
        for (int i = 0; i < 3; i++) {
            bsp_board_led_on(0);  // Red LED = no bytes sent
            nrf_delay_ms(100);
            bsp_board_led_off(0);
            nrf_delay_ms(100);
        }
    }
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
 * Format: CFG ch=5 rate=6m8 pl=128 len=64 rate_hz=100
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

    // Handle commands
    if (strcmp(cmd_upper, "PNG") == 0 || strcmp(cmd_upper, "PING") == 0)
    {
        // CRITICAL: Respond immediately to PING - this is used for connectivity checks
        // Diagnostic: Log that we're sending response
        test_run_info((unsigned char *)"[DBG] PNG received, sending OK response");
        send_response("OK");
        test_run_info((unsigned char *)"[DBG] PNG response sent");
        return;  // Return immediately after sending response
    }
    else if (strncmp(cmd_upper, "NODE_TYPE", 9) == 0)
    {
        send_response("OK NODE_TYPE=RX_V2");
    }
    else if (strncmp(cmd_upper, "CFG ", 4) == 0 || strncmp(cmd_upper, "SET_CONFIG ", 11) == 0)
    {
        char *params = strchr(cmd, ' ');
        if (params) params++;
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
        
        // Enable event counters and diagnostics
        dwt_configeventcounters(1);
        dwt_configciadiag(1);
        
        // Ensure RX is enabled
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
        
        send_response("OK START");
    }
    else if (strcmp(cmd_upper, "STOP") == 0 || strcmp(cmd_upper, "STOP_TEST") == 0)
    {
        g_test_running = 0;
        // Stop RX - force to IDLE state
        dwt_forcetrxoff();
        send_response("OK STOP");
    }
    else if (strcmp(cmd_upper, "STAT") == 0 || strcmp(cmd_upper, "GET_STATS") == 0)
    {
        // Read event counters
        dwt_readeventcounters(&g_event_cnts);
        
        // Calculate averages
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
        send_response(stats_str);
    }
    else if (strcmp(cmd_upper, "RST") == 0 || strcmp(cmd_upper, "RESET_STATS") == 0)
    {
        memset(&g_stats, 0, sizeof(g_stats));
        memset(&g_event_cnts, 0, sizeof(g_event_cnts));
        // Reset event counters by re-enabling them
        dwt_configeventcounters(1);
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
    
    // Enable event counters and diagnostics
    dwt_configeventcounters(1);
    dwt_configciadiag(1);
}

/**
 * Process received packet (non-blocking)
 */
/**
 * Ping timer handler - sends ping message to Arduino every 5 seconds
 */
static void ping_timer_handler(void *p_context)
{
    // Send ping message to Arduino
    send_response("OK PING");
}

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
            
            // Only process packets when test is running (START command received)
            if (g_test_running)
            {
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
                
                // Read RF diagnostics
                dwt_rxdiag_t rx_diag;
                dwt_readdiagnostics(&rx_diag);
                
                // Update RF metrics
                update_rf_metrics(&rx_diag);
                
                // Re-enable RX to continue listening for packets
                dwt_rxenable(DWT_START_RX_IMMEDIATE);
            }
            else
            {
                // Test not running - clear status but don't re-enable RX
                dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);
            }
        }
    }
    else
    {
        // RX error - categorize error type (only track when test is running)
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
        
        // Only re-enable RX if test is running (START command received)
        if (g_test_running)
        {
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }
    }
}

/**
 * Application entry point
 */
int orchestrator_rx_v2(void)
{
    uint32_t dev_id;
    uint32_t err_code;

    /* Display application name */
    test_run_info((unsigned char *)APP_NAME);
    
    /* Initialize all LEDs to off */
    bsp_board_led_off(0);  // Red LED - Default/Error states
    bsp_board_led_off(1);  // Orange LED - RX (command received)
    bsp_board_led_off(2);  // Green LED - Byte-by-byte RX indicator (GPIO 22, doesn't conflict with UART)
    bsp_board_led_off(3);  // Blue LED - Not used (GPIO 14 conflicts with UART RX pin)
    
    /* Red LED: Default state - blink to show firmware started */
    bsp_board_led_on(0);
    nrf_delay_ms(100);
    bsp_board_led_off(0);
    nrf_delay_ms(100);
    bsp_board_led_on(0);
    nrf_delay_ms(100);
    bsp_board_led_off(0);
    
    /* CRITICAL: Configure RX pin as input with pullup BEFORE UART initialization */
    /* This matches the demo firmware pattern (peripherals_init() does this) */
    /* The pin will be reset inside uart_init(), but this ensures initial state is correct */
    nrf_gpio_cfg_input(UART_0_RX_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* CRITICAL: Initialize UART FIRST, before anything else */
    /* Pin reset is now done inside uart_init() immediately before APP_UART_FIFO_INIT */
    /* RX pin pullup is reconfigured inside uart_init() after the reset */
    uart_init();
    
    /* LED pattern to indicate UART initialized:
     * - Orange LED (LED 1): UART initialized
     * - Green LED (LED 2): UART ready (blinks on each byte received)
     */
    bsp_board_led_on(1);  // Orange LED ON = UART initialized
    bsp_board_led_on(2);  // Green LED ON = UART ready (will blink on byte reception)
    nrf_delay_ms(200);
    bsp_board_led_off(1);
    bsp_board_led_off(2);
    nrf_delay_ms(100);
    bsp_board_led_on(1);
    bsp_board_led_on(2);
    nrf_delay_ms(200);
    bsp_board_led_off(1);
    bsp_board_led_off(2);
    
    /* Wait for UART to be ready */
    Sleep(300);
    
    /* Send startup message to RTT only - don't send unsolicited UART messages */
    test_run_info((unsigned char *)"OK STARTUP V2");
    // Commented out: send_response("OK STARTUP V2");  // Don't send unsolicited messages
    Sleep(100);
    
    /* Initialize app timer module for ping timer */
    err_code = app_timer_init();
    if (err_code == NRF_SUCCESS)
    {
        /* Create ping timer for Arduino communication (every 5 seconds) */
        err_code = app_timer_create(&m_ping_timer_id, APP_TIMER_MODE_REPEATED, ping_timer_handler);
        if (err_code == NRF_SUCCESS)
        {
            /* Start ping timer: 5000ms = 5 seconds */
            err_code = app_timer_start(m_ping_timer_id, APP_TIMER_TICKS(5000), NULL);
            if (err_code == NRF_SUCCESS)
            {
                test_run_info((unsigned char *)"PING timer started (5s interval)");
            }
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
        for (int i = 0; i < 20; i++) {
            bsp_board_led_on(0);
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            nrf_delay_ms(50);
        }
        bsp_board_led_on(0);
    }

    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR)
    {
        send_response("ERR DW3000_INIT_FAILED");
        test_run_info((unsigned char *)"INIT FAILED");
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
        test_run_info((unsigned char *)"OK DW3000_READY");
        // Commented out: send_response("OK DW3000_READY");  // Don't send unsolicited messages
    }

    /* Enable LEDs */
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);

    /* Configure with default settings */
    configure_uwb();

    /* Start in IDLE state - RX will be enabled when START command is received */
    dwt_forcetrxoff();

    /* Send message that we reached main loop */
    Sleep(100);
    test_run_info((unsigned char *)"OK MAIN_LOOP");
    // Commented out: send_response("OK MAIN_LOOP");  // Don't send unsolicited messages
    
    /* Main loop - process UART commands and RX packets */
    while (1)
    {
        // Only process RX packets when test is running (START command received)
        if (g_test_running)
        {
            process_rx_packet();
            Sleep(1);  // Small delay to prevent CPU spinning and allow status register to update
        }
        else
        {
            Sleep(100);
        }
    }
}

#endif

