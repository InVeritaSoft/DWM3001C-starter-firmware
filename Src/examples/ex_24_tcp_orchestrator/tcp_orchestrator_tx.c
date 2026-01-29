/*! ----------------------------------------------------------------------------
 *  @file    tcp_orchestrator_tx.c
 *  @brief   TCP-like Orchestrator TX example (Node A) - Reliable UWB transmitter with ACK/retransmission
 *           Based on orchestrator_tx_v2 with TCP-like features:
 *           - ACK packets from RX node
 *           - Retransmission queue for un-ACKed packets
 *           - Timeout handling and retransmission
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

#if defined(TEST_TCP_ORCHESTRATOR_TX)

extern void test_run_info(unsigned char *data);

/* Example application name */
#define APP_NAME "TCP ORCHESTRATOR TX v1.0"

/* UART buffer size */
#define UART_BUFFER_SIZE 256
#define UART_RX_BUFFER_SIZE 256
#define UART_TX_BUFFER_SIZE 256

/* TCP-like retransmission queue size */
#define RETRANSMIT_QUEUE_SIZE 32
#define MAX_RETRIES 3
#define DEFAULT_ACK_TIMEOUT_MS 100

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
    uint8_t preamble_code;   // TX/RX preamble code
    uint32_t ref_tx_power;   // Reference TX power setting (for boost calculation)
    uint16_t power_boost;    // Power boost in 0.1dB steps
    uint32_t pkt_rate_hz;    // TX packets per second
    uint16_t payload_len;    // Payload size in bytes
    uint8_t configured;      // Configuration flag
    
    // TCP-like parameters
    uint8_t tcp_mode;        // TCP mode enabled (1) or disabled (0)
    uint32_t ack_timeout_ms; // ACK timeout in milliseconds
    uint8_t max_retries;     // Maximum retransmission attempts
    uint16_t window_size;    // Sliding window size
} uwb_config_t;

/* TCP-like packet structure */
typedef struct {
    uint32_t seq;             // Sequence number
    uint32_t ack_seq;          // Acknowledged sequence (0 if not ACK)
    uint8_t flags;             // TCP flags (ACK, SYN, FIN, RST)
    uint16_t window;           // Receiver window size
    uint32_t t_local;         // Local timestamp
    uint8_t payload[64];      // Payload data
} __attribute__((packed)) uwb_tcp_packet_t;

/* Retransmission queue entry */
typedef struct {
    uint32_t seq;              // Sequence number
    uint32_t timestamp;        // Send timestamp (for timeout)
    uint8_t retry_count;       // Number of retries
    uint8_t data[FRAME_LEN_MAX]; // Packet data
    uint16_t data_len;        // Packet data length
    uint8_t valid;             // Entry is valid
} retransmit_entry_t;

/* Enhanced statistics structure */
typedef struct {
    uint32_t total_sent;      // Total packets sent successfully
    uint32_t total_attempted; // Total transmission attempts
    uint32_t tx_errors;       // Transmission errors
    uint32_t tx_timeouts;     // TX timeout errors
    uint32_t last_error;      // Last error code
    uint32_t last_tx_timestamp; // Last successful TX timestamp
    uint16_t frame_duration_us; // Calculated frame duration
    
    // TCP-like statistics
    uint32_t acks_received;   // Number of ACKs received
    uint32_t acks_missing;    // Number of missing ACKs (timeouts)
    uint32_t retransmissions; // Number of retransmitted packets
    uint32_t packets_in_flight; // Current packets waiting for ACK
} tx_stats_t;

/* Global state */
static uwb_config_t g_config = {
    .channel = 5,
    .data_rate = 1,           // 6.8Mbps
    .preamble_len = 128,
    .preamble_code = 9,
    .ref_tx_power = 0x36363636, // Default reference power (calibrated value)
    .power_boost = 0,         // No boost by default
    .pkt_rate_hz = 100,
    .payload_len = 64,
    .configured = 0,
    .tcp_mode = 1,           // TCP mode enabled by default
    .ack_timeout_ms = DEFAULT_ACK_TIMEOUT_MS,
    .max_retries = MAX_RETRIES,
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

static tx_stats_t g_stats = {0};
static volatile uint8_t g_test_running = 0;
static volatile uint8_t g_tx_in_progress = 0; // Prevent overlapping TX calls
static volatile uint32_t g_seq_num = 0;
static uint8_t g_tx_buffer[FRAME_LEN_MAX];
static uwb_tcp_packet_t g_tx_packet;
static uint8_t g_timer_initialized = 0;
static dwt_txconfig_t g_tx_config; // TX power configuration

/* Retransmission queue */
static retransmit_entry_t g_retransmit_queue[RETRANSMIT_QUEUE_SIZE];
static uint8_t g_queue_head = 0;
static uint8_t g_queue_tail = 0;
static uint8_t g_queue_count = 0;

/* Timer for periodic TX */
APP_TIMER_DEF(m_tx_timer_id);
/* Timer for ping-pong messages to Arduino (every 5 seconds) */
APP_TIMER_DEF(m_ping_timer_id);
/* Timer for ACK timeout checking */
APP_TIMER_DEF(m_ack_timeout_timer_id);

/* Forward declarations */
static void uart_event_handler(app_uart_evt_t *p_event);
static uint32_t uart_init(void);
static void parse_command(char *cmd);
static void send_response(const char *response);
static void send_test_char(char c);
static void configure_uwb(void);
static void configure_tx_power(void);
static uint16_t calculate_frame_duration_us(void);
static void tx_timer_handler(void *p_context);
static void ping_timer_handler(void *p_context);
static void ack_timeout_timer_handler(void *p_context);
static void send_packet(void);
static void process_ack_packet(uwb_tcp_packet_t *ack_packet);
static void check_retransmit_timeouts(void);
static void add_to_retransmit_queue(uint32_t seq, uint8_t *data, uint16_t len);
static void remove_from_retransmit_queue(uint32_t ack_seq);
static void retransmit_packet(retransmit_entry_t *entry);

/**
 * Calculate frame duration in microseconds
 */
static uint16_t calculate_frame_duration_us(void)
{
    uint16_t duration = 0;
    uint16_t symbol_duration_us;
    if (g_config.data_rate == 1) // 6.8Mbps
    {
        symbol_duration_us = 1024;
    }
    else // 850kbps
    {
        symbol_duration_us = 8192;
    }
    
    duration = (g_config.preamble_len * symbol_duration_us) / 1024;
    duration += (8 * symbol_duration_us) / 1024; // SFD
    duration += (8 * symbol_duration_us) / 1024; // PHY header
    duration += (g_config.payload_len * 8 * symbol_duration_us) / 1024;
    duration += (16 * symbol_duration_us) / 1024; // FCS
    
    return duration;
}

/**
 * Add packet to retransmission queue
 */
static void add_to_retransmit_queue(uint32_t seq, uint8_t *data, uint16_t len)
{
    if (g_queue_count >= RETRANSMIT_QUEUE_SIZE)
    {
        // Queue full - remove oldest entry
        g_queue_head = (g_queue_head + 1) % RETRANSMIT_QUEUE_SIZE;
        g_queue_count--;
    }
    
    retransmit_entry_t *entry = &g_retransmit_queue[g_queue_tail];
    entry->seq = seq;
    entry->timestamp = dwt_readsystimestamphi32();
    entry->retry_count = 0;
    entry->data_len = len;
    entry->valid = 1;
    memcpy(entry->data, data, len);
    
    g_queue_tail = (g_queue_tail + 1) % RETRANSMIT_QUEUE_SIZE;
    g_queue_count++;
    g_stats.packets_in_flight++;
}

/**
 * Remove ACKed packets from retransmission queue
 */
static void remove_from_retransmit_queue(uint32_t ack_seq)
{
    uint8_t removed = 0;
    for (uint8_t i = 0; i < RETRANSMIT_QUEUE_SIZE; i++)
    {
        retransmit_entry_t *entry = &g_retransmit_queue[i];
        if (entry->valid && entry->seq <= ack_seq)
        {
            entry->valid = 0;
            g_queue_count--;
            g_stats.packets_in_flight--;
            removed++;
        }
    }
    
    // Compact queue by moving valid entries forward
    if (removed > 0)
    {
        uint8_t write_idx = 0;
        for (uint8_t i = 0; i < RETRANSMIT_QUEUE_SIZE; i++)
        {
            if (g_retransmit_queue[i].valid)
            {
                if (write_idx != i)
                {
                    memcpy(&g_retransmit_queue[write_idx], &g_retransmit_queue[i], sizeof(retransmit_entry_t));
                    g_retransmit_queue[i].valid = 0;
                }
                write_idx++;
            }
        }
        g_queue_head = 0;
        g_queue_tail = write_idx;
    }
}

/**
 * Retransmit a packet from the queue
 */
static void retransmit_packet(retransmit_entry_t *entry)
{
    if (!entry->valid) return;
    
    entry->retry_count++;
    entry->timestamp = dwt_readsystimestamphi32();
    g_stats.retransmissions++;
    
    // Copy packet data to TX buffer
    memcpy(g_tx_buffer, entry->data, entry->data_len);
    
    // Calculate frame length
    uint16_t frame_len = entry->data_len + FCS_LEN;
    
    // Write TX data
    dwt_writetxdata(entry->data_len, g_tx_buffer, 0);
    dwt_writetxfctrl(frame_len, 0, 0);
    
    // Ensure DW3000 is ready
    dwt_forcetrxoff();
    
    // Start transmission
    dwt_starttx(DWT_START_TX_IMMEDIATE);
    
    // Poll for TX complete with timeout to prevent blocking
    uint32_t status_reg;
    uint32_t timeout_count = 0;
    const uint32_t max_timeout_ms = 20;
    
    status_reg = dwt_readsysstatuslo();
    while (!(status_reg & DWT_INT_TXFRS_BIT_MASK) && timeout_count < max_timeout_ms)
    {
        Sleep(1); // Sleep 1ms
        timeout_count++;
        status_reg = dwt_readsysstatuslo();
    }
    
    if (status_reg & DWT_INT_TXFRS_BIT_MASK)
    {
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
    }
    else if (timeout_count >= max_timeout_ms)
    {
        // Timeout occurred - force to IDLE and mark for removal
        dwt_forcetrxoff();
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK | DWT_INT_TXFRB_BIT_MASK | DWT_INT_TXPRS_BIT_MASK);
    }
    
    // If max retries exceeded, remove from queue
    if (entry->retry_count >= g_config.max_retries)
    {
        entry->valid = 0;
        g_queue_count--;
        g_stats.packets_in_flight--;
        g_stats.tx_timeouts++;
    }
}

/**
 * Check for ACK timeouts and retransmit
 */
static void check_retransmit_timeouts(void)
{
    if (!g_config.tcp_mode || g_queue_count == 0) return;
    
    uint32_t current_time = dwt_readsystimestamphi32();
    uint32_t timeout_ticks = (g_config.ack_timeout_ms * 1000) / 64; // Convert ms to DW3000 ticks (64MHz / 128 = ~500kHz)
    
    for (uint8_t i = 0; i < RETRANSMIT_QUEUE_SIZE; i++)
    {
        retransmit_entry_t *entry = &g_retransmit_queue[i];
        if (entry->valid)
        {
            uint32_t elapsed = current_time - entry->timestamp;
            if (elapsed > timeout_ticks && entry->retry_count < g_config.max_retries)
            {
                retransmit_packet(entry);
            }
        }
    }
}

/**
 * Process ACK packet received from RX node
 */
static void process_ack_packet(uwb_tcp_packet_t *ack_packet)
{
    if (!g_config.tcp_mode) return;
    
    if (ack_packet->flags & TCP_FLAG_ACK)
    {
        g_stats.acks_received++;
        remove_from_retransmit_queue(ack_packet->ack_seq);
        
        char log_buf[64];
        snprintf(log_buf, sizeof(log_buf), "[TCP] ACK received for seq=%lu", (unsigned long)ack_packet->ack_seq);
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
            g_stats.tx_errors++;
            break;
        }
        case APP_UART_FIFO_ERROR:
        {
            NRF_UART0->ERRORSRC = 0xFFFFFFFF;
            rx_index = 0;
            g_stats.tx_errors++;
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
        // Don't leave red LED on - continue anyway
    }
    else
    {
        test_run_info((unsigned char *)"UART INIT OK");
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
    
    // CRITICAL: Enable RS-485 transmit mode BEFORE sending data (if DE pin defined)
    #ifdef RS485_DE_PIN
    nrf_gpio_pin_set(RS485_DE_PIN);  // Set DE HIGH = transmit mode
    nrf_delay_us(50);  // Wait for transceiver to switch to transmit mode (typ. 10-30us)
    #endif
    
    // GREEN LED: TX - Response being sent (turn on at start)
    // NOTE: Green LED is also used for byte-by-byte RX indication, but TX takes priority
    bsp_board_led_on(2);
    
    // Send response string
    // CRITICAL: Balance between non-blocking and actually sending the response
    // 5ms timeout allows buffer to clear while still being fast enough to not block significantly
    // Called from interrupt context, so must be reasonably fast
    // NOTE: Removed diagnostic logging from this function to avoid UART TX buffer conflicts
    for (uint32_t i = 0; i < len; i++)
    {
        timeout = 5;  // 5ms timeout - balance between non-blocking and success
        uint32_t uart_result;
        while ((uart_result = app_uart_put(response[i])) != NRF_SUCCESS && timeout > 0)
        {
            timeout--;
            Sleep(1);
        }
        if (timeout == 0) {
            // TX failed - blink red LED to indicate error and exit early
            bsp_board_led_off(2);
            bsp_board_led_on(0);  // Red LED = error
            nrf_delay_ms(50);
            bsp_board_led_off(0);
            // Don't log here - would compete for UART TX buffer
            return;  // Exit early on failure
        }
        bytes_sent++;
    }
    
    // Send carriage return
    timeout = 5;  // 5ms timeout
    while (app_uart_put('\r') != NRF_SUCCESS && timeout > 0)
    {
        timeout--;
        Sleep(1);
    }
    if (timeout > 0) {
        bytes_sent++;
    }
    
    // Send newline
    timeout = 5;  // 5ms timeout
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
    // CRITICAL: Use minimal delay to avoid blocking UART interrupt handler
    nrf_delay_ms(1);  // Reduced from 5ms to 1ms to prevent blocking interrupt handler
    
    // CRITICAL: Disable RS-485 transmit mode AFTER sending data (if DE pin defined)
    #ifdef RS485_DE_PIN
    nrf_gpio_pin_clear(RS485_DE_PIN);  // Set DE LOW = receive mode
    nrf_delay_us(50);  // Wait for transceiver to switch to receive mode
    #endif
    
    // CRITICAL: Turn off LED immediately - do NOT delay here as it blocks interrupt handler
    // The LED was already on during transmission, which is sufficient indication
    bsp_board_led_off(2);
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
 * Configure TX power with boost calculation
 */
static void configure_tx_power(void)
{
    extern dwt_txconfig_t txconfig_options;
    uint32_t adj_tx_power;
    uint16_t applied_boost;
    int err;
    
    uint16_t frame_duration_us = calculate_frame_duration_us();
    uint16_t calculated_boost = calculate_power_boost(frame_duration_us);
    
    uint16_t boost_to_apply = (g_config.power_boost < calculated_boost) ? 
                               g_config.power_boost : calculated_boost;
    
    err = dwt_adjust_tx_power(boost_to_apply, g_config.ref_tx_power, 
                              g_config.channel, &adj_tx_power, &applied_boost);
    
    if (err == DWT_ERROR)
    {
        g_tx_config = txconfig_options;
        test_run_info((unsigned char *)"TX_PWR_ADJ_FAIL");
    }
    else
    {
        g_tx_config.power = adj_tx_power;
        g_tx_config.PGcount = txconfig_options.PGcount;
        g_tx_config.PGdly = txconfig_options.PGdly;
    }
    
    dwt_configuretxrf(&g_tx_config);
}

/**
 * Parse SET_CONFIG command
 * Format: CFG ch=5 rate=6m8 pl=128 len=64 pwr_ref=0x36363636 boost=0 rate_hz=100 tcp_mode=1 ack_timeout=100 max_retries=3 window_size=10
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
        
        // Send response immediately after parsing (before configure_uwb which can take time)
        send_response("OK CONFIG");
        
        // Then perform the actual configuration (this may take several seconds)
        configure_uwb();
        g_config.configured = 1;
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
        else if (strncmp(token, "pwr_ref=", 8) == 0)
        {
            g_config.ref_tx_power = (uint32_t)strtoul(token + 8, NULL, 16);
        }
        else if (strncmp(token, "boost=", 6) == 0)
        {
            g_config.power_boost = (uint16_t)atoi(token + 6);
        }
        else if (strncmp(token, "rate_hz=", 8) == 0)
        {
            g_config.pkt_rate_hz = (uint32_t)atoi(token + 8);
        }
        else if (strncmp(token, "tcp_mode=", 9) == 0)
        {
            g_config.tcp_mode = (uint8_t)atoi(token + 9);
        }
        else if (strncmp(token, "ack_timeout=", 12) == 0)
        {
            g_config.ack_timeout_ms = (uint32_t)atoi(token + 12);
        }
        else if (strncmp(token, "max_retries=", 12) == 0)
        {
            g_config.max_retries = (uint8_t)atoi(token + 12);
        }
        else if (strncmp(token, "window_size=", 12) == 0)
        {
            g_config.window_size = (uint16_t)atoi(token + 12);
        }

        token = strtok(NULL, " ");
    }

    if (config_valid)
    {
        // Response already sent at start of function to prevent timeout
        // Now perform the actual configuration (this may take several seconds)
        configure_uwb();
        g_config.configured = 1;
    }
    else
    {
        // If config was invalid, we already sent OK CONFIG, but that's okay
        // The configuration will still be attempted with parsed values
        configure_uwb();
        g_config.configured = 1;
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
    
    // Convert to uppercase (removed diagnostic logging to avoid UART TX buffer conflicts)
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

    if (strcmp(cmd_upper, "PNG") == 0 || strcmp(cmd_upper, "PING") == 0)
    {
        // Removed diagnostic logging to avoid UART TX buffer conflicts
        send_response("OK");
        return;
    }
    else if (strncmp(cmd_upper, "NODE_TYPE", 9) == 0)
    {
        // Removed diagnostic logging to avoid UART TX buffer conflicts
        send_response("OK NODE_TYPE=TX_V2");
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
        // Debug: log START command received
        char log_buf[128];
        test_run_info((unsigned char *)"[DBG] START command received");
        snprintf(log_buf, sizeof(log_buf), "[DBG] configured=%d timer_init=%d", g_config.configured, g_timer_initialized);
        test_run_info((unsigned char *)log_buf);
        
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
        // Don't reset stats on START - let them accumulate (matches orchestrator_v2 behavior)
        // Only reset if explicitly requested via RESET_STATS command
        
        // Clear retransmission queue
        memset(g_retransmit_queue, 0, sizeof(g_retransmit_queue));
        g_queue_head = 0;
        g_queue_tail = 0;
        g_queue_count = 0;

        // Calculate timer period based on packet rate
        uint32_t period_ms = 1000;
        if (g_config.pkt_rate_hz > 0)
        {
            period_ms = 1000 / g_config.pkt_rate_hz;
        }
        if (period_ms < 1) period_ms = 1;
        if (period_ms > 1000) period_ms = 1000; // Cap at 1 second max
        
        // Debug: log timer period
        snprintf(log_buf, sizeof(log_buf), "[DBG] Starting TX timer: period_ms=%lu pkt_rate_hz=%lu", 
                 (unsigned long)period_ms, (unsigned long)g_config.pkt_rate_hz);
        test_run_info((unsigned char *)log_buf);
        
        uint32_t err_code = app_timer_start(m_tx_timer_id, APP_TIMER_TICKS(period_ms), NULL);

        if (err_code == NRF_SUCCESS)
        {
            // Start ACK timeout timer if TCP mode enabled
            if (g_config.tcp_mode)
            {
                err_code = app_timer_start(m_ack_timeout_timer_id, APP_TIMER_TICKS(g_config.ack_timeout_ms), NULL);
            }
            send_response("OK START");
        }
        else
        {
            send_response("ERR START_FAILED");
        }
    }
    else if (strcmp(cmd_upper, "STOP") == 0 || strcmp(cmd_upper, "STOP_TEST") == 0)
    {
        // Send response IMMEDIATELY to prevent timeout (before any cleanup that might take time)
        send_response("OK STOP");
        
        // Stop test running flag to prevent timer handler from starting new TX
        g_test_running = 0;
        
        // Stop timers immediately
        app_timer_stop(m_tx_timer_id);
        app_timer_stop(m_ack_timeout_timer_id);
        
        // Force DW3000 to IDLE state to stop any transmission
        dwt_forcetrxoff();
        
        // Clear any pending status bits
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK | DWT_INT_TXFRB_BIT_MASK | DWT_INT_TXPRS_BIT_MASK);
        
        // Reset TX in progress flag in case it was stuck (non-blocking)
        g_tx_in_progress = 0;
    }
    else if (strcmp(cmd_upper, "STAT") == 0 || strcmp(cmd_upper, "GET_STATS") == 0 || strcmp(cmd_upper, "STATS") == 0)
    {
        char stats_str[512];
        if (g_config.tcp_mode)
        {
            snprintf(stats_str, sizeof(stats_str), 
                     "OK STATS sent=%lu attempted=%lu errors=%lu timeouts=%lu last_err=%lu frame_dur=%u "
                     "acks_rcvd=%lu acks_missing=%lu retrans=%lu in_flight=%lu",
                     (unsigned long)g_stats.total_sent,
                     (unsigned long)g_stats.total_attempted,
                     (unsigned long)g_stats.tx_errors,
                     (unsigned long)g_stats.tx_timeouts,
                     (unsigned long)g_stats.last_error,
                     (unsigned int)g_stats.frame_duration_us,
                     (unsigned long)g_stats.acks_received,
                     (unsigned long)g_stats.acks_missing,
                     (unsigned long)g_stats.retransmissions,
                     (unsigned long)g_stats.packets_in_flight);
        }
        else
        {
            snprintf(stats_str, sizeof(stats_str), 
                     "OK STATS sent=%lu attempted=%lu errors=%lu timeouts=%lu last_err=%lu frame_dur=%u",
                     (unsigned long)g_stats.total_sent,
                     (unsigned long)g_stats.total_attempted,
                     (unsigned long)g_stats.tx_errors,
                     (unsigned long)g_stats.tx_timeouts,
                     (unsigned long)g_stats.last_error,
                     (unsigned int)g_stats.frame_duration_us);
        }
        send_response(stats_str);
    }
    else if (strcmp(cmd_upper, "RST") == 0 || strcmp(cmd_upper, "RESET_STATS") == 0)
    {
        g_stats.total_sent = 0;
        g_stats.total_attempted = 0;
        g_stats.tx_errors = 0;
        g_stats.tx_timeouts = 0;
        g_stats.last_error = 0;
        g_stats.acks_received = 0;
        g_stats.acks_missing = 0;
        g_stats.retransmissions = 0;
        g_stats.packets_in_flight = 0;
        g_seq_num = 0;
        memset(g_retransmit_queue, 0, sizeof(g_retransmit_queue));
        g_queue_head = 0;
        g_queue_tail = 0;
        g_queue_count = 0;
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

    configure_tx_power();
    g_stats.frame_duration_us = calculate_frame_duration_us();
}

/**
 * TX timer handler
 */
static void tx_timer_handler(void *p_context)
{
    // Always check if test is running - don't block timer if test stopped
    if (!g_test_running)
    {
        return;
    }
    
    // Prevent overlapping TX calls - if previous TX is still in progress, skip this one
    // This prevents blocking the timer handler if send_packet() takes longer than timer period
    if (g_tx_in_progress)
    {
        // Previous TX still in progress - skip this timer tick to prevent blocking
        return;
    }
    
    // Check window size limit in TCP mode
    if (g_config.tcp_mode && g_stats.packets_in_flight >= g_config.window_size)
    {
        // Window full - skip this transmission but keep timer running
        g_stats.acks_missing++;
        return;
    }
    
    // Send packet - this should not block the timer handler
    // Note: send_packet() checks g_test_running internally and exits early if stopped
    send_packet();
}

/**
 * ACK timeout timer handler
 */
static void ack_timeout_timer_handler(void *p_context)
{
    if (g_test_running && g_config.tcp_mode)
    {
        check_retransmit_timeouts();
    }
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
 * Send a test packet
 */
static void send_packet(void)
{
    // Check if test is still running - exit early if stopped
    if (!g_test_running)
    {
        return;
    }
    
    // Prevent overlapping calls
    if (g_tx_in_progress)
    {
        return;
    }
    g_tx_in_progress = 1;
    
    // Double-check test is still running after acquiring lock
    if (!g_test_running)
    {
        g_tx_in_progress = 0;
        return;
    }
    
    uint32_t status_reg;
    uint32_t timeout_count = 0;
    // Maximum wait time: 20ms (should be enough for TX, but prevents blocking timer)
    // For 100Hz rate (10ms period), this allows 2x margin
    const uint32_t max_timeout_ms = 20;

    g_stats.total_attempted++;
    
    // Ensure DW3000 is in a good state before starting TX
    if (!dwt_checkidlerc())
    {
        // DW3000 not ready - force to IDLE and try again
        dwt_forcetrxoff();
        Sleep(1);
        // If still not ready, skip this transmission but keep timer running
        if (!dwt_checkidlerc())
        {
            g_stats.tx_errors++;
            g_tx_in_progress = 0;
            return;
        }
    }

    // Prepare TCP-like packet
    g_tx_packet.seq = g_seq_num++;
    g_tx_packet.ack_seq = 0;  // No ACK in data packets
    g_tx_packet.flags = 0;    // No flags for data packets
    g_tx_packet.window = g_config.window_size;
    g_tx_packet.t_local = dwt_readsystimestamphi32();
    
    // Fill payload
    for (int i = 0; i < g_config.payload_len && i < sizeof(g_tx_packet.payload); i++)
    {
        g_tx_packet.payload[i] = 0xAA;
    }

    // Calculate packet size based on actual payload length (not full structure size)
    // Header: seq(4) + ack_seq(4) + flags(1) + window(2) + t_local(4) = 15 bytes
    uint16_t header_len = sizeof(uwb_tcp_packet_t) - sizeof(g_tx_packet.payload);
    uint16_t data_len = header_len + g_config.payload_len;
    uint16_t frame_len = data_len + FCS_LEN;
    
    // Diagnostic: Log frame length calculation (first few attempts only to avoid spam)
    if (g_stats.total_attempted <= 3)
    {
        char log_buf[128];
        snprintf(log_buf, sizeof(log_buf), "[DBG] TX: header=%u payload=%u data=%u frame=%u", 
                 header_len, g_config.payload_len, data_len, frame_len);
        test_run_info((unsigned char *)log_buf);
    }
    
    // Copy only the actual data length (not full structure with unused payload bytes)
    memcpy(g_tx_buffer, &g_tx_packet, data_len);

    // Write TX data and frame control (matches ex_22_orchestrator_v2 order)
    dwt_writetxdata(data_len, g_tx_buffer, 0);
    dwt_writetxfctrl(frame_len, 0, 0);

    // Ensure DW3000 is ready for transmission (matches ex_22_orchestrator_v2)
    // Force to IDLE state AFTER writing data but BEFORE starting TX
    dwt_forcetrxoff();
    
    // CRITICAL: Verify IDLE state and wait if needed (prevents TX frame rejection)
    // DW3000 needs time to transition to IDLE after forcetrxoff()
    uint32_t idle_check_count = 0;
    uint8_t was_idle = dwt_checkidlerc();
    while (!dwt_checkidlerc() && idle_check_count < 10)
    {
        Sleep(1);
        idle_check_count++;
    }
    
    // Diagnostic: Log IDLE state (first few attempts only)
    if (g_stats.total_attempted <= 3)
    {
        char log_buf[128];
        snprintf(log_buf, sizeof(log_buf), "[DBG] TX: was_idle=%u after_force_idle=%u wait_count=%lu", 
                 was_idle, dwt_checkidlerc(), (unsigned long)idle_check_count);
        test_run_info((unsigned char *)log_buf);
    }
    
    // Clear any pending status bits BEFORE starting TX to prevent interference
    // Old status bits (especially TXFRB/TXPRS from previous errors) can cause immediate rejection
    dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK | DWT_INT_TXFRB_BIT_MASK | DWT_INT_TXPRS_BIT_MASK);

    // Start transmission
    dwt_starttx(DWT_START_TX_IMMEDIATE);

    // Poll for TX complete with timeout to prevent blocking timer handler
    // Use manual polling instead of waitforsysstatus to allow timeout
    // Check for both success (TXFRS) and error conditions (TXFRB, TXPRS)
    status_reg = dwt_readsysstatuslo();
    uint32_t tx_error_mask = DWT_INT_TXFRB_BIT_MASK | DWT_INT_TXPRS_BIT_MASK;
    
    // Poll with timeout, but check g_test_running frequently to allow STOP command to be processed
    while (!(status_reg & (DWT_INT_TXFRS_BIT_MASK | tx_error_mask)) && timeout_count < max_timeout_ms)
    {
        // Check if test was stopped - exit early to allow STOP command processing
        if (!g_test_running)
        {
            // Force to IDLE and clear flag
            dwt_forcetrxoff();
            dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK | DWT_INT_TXFRB_BIT_MASK | DWT_INT_TXPRS_BIT_MASK);
            g_tx_in_progress = 0;
            return;
        }
        
        Sleep(1); // Sleep 1ms
        timeout_count++;
        status_reg = dwt_readsysstatuslo();
    }

    // Check for TX success
    if (status_reg & DWT_INT_TXFRS_BIT_MASK)
    {
        // Clear TX frame sent event
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK);
        g_stats.total_sent++;
        g_stats.last_tx_timestamp = dwt_readsystimestamphi32();
        g_stats.last_error = 0;
        
        // Add to retransmission queue if TCP mode enabled
        if (g_config.tcp_mode)
        {
            add_to_retransmit_queue(g_tx_packet.seq, g_tx_buffer, data_len);
        }
    }
    // Check for TX errors
    else if (status_reg & tx_error_mask)
    {
        // TX error occurred (TXFRB = TX frame rejected, TXPRS = TX preamble rejected)
        g_stats.tx_errors++;
        if (status_reg & DWT_INT_TXFRB_BIT_MASK)
        {
            g_stats.last_error = 3; // TX frame rejected
        }
        else if (status_reg & DWT_INT_TXPRS_BIT_MASK)
        {
            g_stats.last_error = 4; // TX preamble rejected
        }
        else
        {
            g_stats.last_error = 1; // General TX error
        }
        
        // Clear error bits and force to IDLE
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK | DWT_INT_TXFRB_BIT_MASK | DWT_INT_TXPRS_BIT_MASK);
        dwt_forcetrxoff(); // Force to IDLE state to recover
        
        // Small delay to allow DW3000 to recover from error state
        // This helps prevent rapid-fire errors that could overwhelm the chip
        Sleep(1);
    }
    // Timeout occurred
    else
    {
        g_stats.tx_timeouts++;
        g_stats.last_error = 2; // TX timeout error
        
        // Clear any pending status bits and force to IDLE (matches ex_22_orchestrator_v2 - no extra delay)
        dwt_writesysstatuslo(DWT_INT_TXFRS_BIT_MASK | DWT_INT_TXFRB_BIT_MASK | DWT_INT_TXPRS_BIT_MASK);
        dwt_forcetrxoff(); // Force to IDLE state to recover
    }
    
    // Clear TX in progress flag
    g_tx_in_progress = 0;
}

/**
 * Process received UWB packet (for ACK packets from RX node)
 */
static void process_uwb_rx(void)
{
    // Only process if DW3000 is ready
    if (!dwt_checkidlerc())
    {
        return;
    }
    
    uint32_t status_reg = dwt_readsysstatuslo();
    
    if (status_reg & DWT_INT_RXFCG_BIT_MASK)
    {
        uint16_t frame_len = dwt_getframelength();
        if (frame_len <= FRAME_LEN_MAX)
        {
            uint8_t rx_buffer[FRAME_LEN_MAX];
            dwt_readrxdata(rx_buffer, frame_len - FCS_LEN, 0);
            dwt_writesysstatuslo(DWT_INT_RXFCG_BIT_MASK);
            
            uwb_tcp_packet_t *rx_packet = (uwb_tcp_packet_t *)rx_buffer;
            
            // Process ACK packet
            if (rx_packet->flags & TCP_FLAG_ACK)
            {
                process_ack_packet(rx_packet);
            }
            
            // Re-enable RX to continue listening for ACK packets
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }
    }
    else if (status_reg & SYS_STATUS_ALL_RX_ERR)
    {
        dwt_writesysstatuslo(SYS_STATUS_ALL_RX_ERR);
        // Only re-enable RX if DW3000 is still ready (avoid error loops)
        if (dwt_checkidlerc())
        {
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
        }
    }
}

/**
 * Application entry point
 */
int tcp_orchestrator_tx(void)
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
        err_code = app_timer_create(&m_tx_timer_id, APP_TIMER_MODE_REPEATED, tx_timer_handler);
        if (err_code == NRF_SUCCESS)
        {
            g_timer_initialized = 1;
        }
        
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
        
        // Create ACK timeout timer
        err_code = app_timer_create(&m_ack_timeout_timer_id, APP_TIMER_MODE_REPEATED, ack_timeout_timer_handler);
        if (err_code == NRF_SUCCESS)
        {
            test_run_info((unsigned char *)"ACK timeout timer created");
        }
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

    uint8_t dw3000_ready = 0;
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
        dw3000_ready = 1;
        
        // Configure UWB before enabling LEDs
        configure_uwb();
        dwt_forcetrxoff();
        
        // Disable DW3000 LEDs completely to prevent infinite D13 flashing
        // D13 is the DW3000 clock pin and LED control causes continuous flashing
        dwt_setleds(DWT_LEDS_DISABLE);
        // Enable RX to receive ACK packets
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
    }

    Sleep(100);
    test_run_info((unsigned char *)"OK MAIN_LOOP");
    
    while (1)
    {
        // Process UWB RX (for ACK packets) - only if DW3000 is ready
        if (dw3000_ready)
        {
            process_uwb_rx();
            
            // Check retransmission timeouts
            if (g_config.tcp_mode)
            {
                check_retransmit_timeouts();
            }
        }
        
        Sleep(10);
    }
}

#endif
