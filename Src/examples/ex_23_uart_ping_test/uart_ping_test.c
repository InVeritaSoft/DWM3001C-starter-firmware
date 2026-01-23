/*! ----------------------------------------------------------------------------
 *  @file    uart_ping_test.c
 *  @brief   Simple UART ping test for DWM3001CDK
 *           - Sends "Hello World" when SW2 button is pressed
 *           - Listens for "PING" response
 *           - Lights orange LED when "PING" is received
 *
 * @attention
 *
 * Copyright 2015 - 2021 (c) Decawave Ltd, Dublin, Ireland.
 *
 * All rights reserved.
 *
 * @author Generated for INVERITA DWM3001C Test Rig
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

#if defined(TEST_UART_PING)

extern void test_run_info(unsigned char *data);

/* Example application name */
#define APP_NAME "UART PING TEST"

/* UART buffer size */
#define UART_RX_BUFFER_SIZE 64
#define UART_TX_BUFFER_SIZE 64

/* Message strings */
#define MSG_HELLO_WORLD "Hello World\r\n"
#define MSG_PING "PING"
#define MSG_PING_LEN 4

/* Global state */
static char rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t rx_index = 0;
static volatile uint8_t ping_received = 0;
static uint8_t last_button_state = 1;  // Button is HIGH when not pressed (pullup)

/* Forward declarations */
static void uart_event_handler(app_uart_evt_t *p_event);
static void uart_init(void);

/**
 * UART event handler
 */
static void uart_event_handler(app_uart_evt_t *p_event)
{
    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
        {
            uint8_t data;
            while (app_uart_get(&data) == NRF_SUCCESS)
            {
                if (rx_index < (UART_RX_BUFFER_SIZE - 1))
                {
                    rx_buffer[rx_index++] = data;
                    rx_buffer[rx_index] = '\0'; // Null terminate for string functions
                    
                    // Check if we received "PING" anywhere in the buffer
                    if (strstr((char *)rx_buffer, MSG_PING) != NULL)
                    {
                        ping_received = 1;
                        rx_index = 0; // Reset buffer
                        rx_buffer[0] = '\0';
                    }
                    // If buffer is getting full or we got a newline, reset
                    else if (data == '\n' || data == '\r' || rx_index >= (UART_RX_BUFFER_SIZE - 2))
                    {
                        rx_index = 0;
                        rx_buffer[0] = '\0';
                    }
                }
                else
                {
                    // Buffer full, reset
                    rx_index = 0;
                    rx_buffer[0] = '\0';
                }
            }
            break;
        }
        case APP_UART_COMMUNICATION_ERROR:
        {
            NRF_UART0->ERRORSRC = 0xFFFFFFFF;  // Clear all error sources
            rx_index = 0;
            break;
        }
        case APP_UART_FIFO_ERROR:
        {
            NRF_UART0->ERRORSRC = 0xFFFFFFFF;  // Clear all error sources
            rx_index = 0;
            break;
        }
        default:
            break;
    }
}

/**
 * Initialize UART for communication with Arduino
 * Uses J10 connector pins:
 * - TX: GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX)
 * - RX: GPIO 15 (P0.15) - J10 Pin 10 (left side)
 */
static void uart_init(void)
{
    uint32_t err_code;
    uint32_t *p_err_code = &err_code;
    
    app_uart_comm_params_t comm_params =
    {
        .rx_pin_no = UART_0_RX_PIN,  // GPIO 15 (P0.15) - J10 Pin 10 (left side - RX)
        .tx_pin_no = UART_0_TX_PIN,  // GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX)
        .rts_pin_no = 4,             // P0.4 (LED 1) - unused for flow control
        .cts_pin_no = 5,             // P0.5 (LED 2) - unused for flow control
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity = false,
        .baud_rate = 15400960  // 57600 baud (Arduino bridge expects 57600)
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
        bsp_board_led_on(0);
    }
    else
    {
        test_run_info((unsigned char *)"UART INIT SUCCESS");
    }
}

/**
 * Main function
 */
int uart_ping_test(void)
{
    test_run_info((unsigned char *)APP_NAME);
    test_run_info((unsigned char *)"Starting UART ping test...");
    
    // Initialize LEDs and Buttons
    bsp_board_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS);
    
    // Blink red LED 3 times on startup
    for (int i = 0; i < 3; i++) {
        bsp_board_led_on(0);
        nrf_delay_ms(200);
        bsp_board_led_off(0);
        nrf_delay_ms(200);
    }
    
    // Initialize UART
    uart_init();
    
    // Turn on blue LED to indicate UART initialized
    bsp_board_led_on(3);  // Blue LED (BSP_LED_3)
    nrf_delay_ms(500);
    bsp_board_led_off(3);
    
    test_run_info((unsigned char *)"Press SW2 to send Hello World...");
    
    // Initialize button state (read current state)
    last_button_state = nrf_gpio_pin_read(BSP_BUTTON_0);
    
    while (1)
    {
        // Check button state (SW2 = BSP_BUTTON_0)
        uint8_t current_button_state = nrf_gpio_pin_read(BSP_BUTTON_0);
        
        // Detect button press (edge detection: HIGH -> LOW)
        // BUTTONS_ACTIVE_STATE is 0, so button is LOW when pressed
        if (last_button_state == 1 && current_button_state == 0)
        {
            // Button was just pressed - debounce by waiting a bit
            nrf_delay_ms(50);
            
            // Check again to confirm it's still pressed (debouncing)
            if (nrf_gpio_pin_read(BSP_BUTTON_0) == 0)
            {
                // Send "Hello World" message
                const char *msg = MSG_HELLO_WORLD;
                while (*msg)
                {
                    while (app_uart_put(*msg) != NRF_SUCCESS)
                    {
                        // Wait for UART to be ready
                        nrf_delay_ms(1);
                    }
                    msg++;
                }
                
                test_run_info((unsigned char *)"Sent: Hello World (SW2 pressed)");
                
                // Blue LED feedback on button press (TX - sending data)
                // BSP_LED_0 = 4 (Red, D9)
                // BSP_LED_1 = 5 (Orange, D10)
                // BSP_LED_2 = 22 (Green, D11)
                // BSP_LED_3 = 14 (Blue, D12) ← Blue LED for TX
                bsp_board_led_on(3);  // Blue LED (BSP_LED_3 = D12) - TX indicator
                nrf_delay_ms(200);
                bsp_board_led_off(3);
            }
        }
        
        // Update last button state
        last_button_state = current_button_state;
        
        // Check if PING was received
        if (ping_received)
        {
            ping_received = 0;
            
            // Light orange LED when PING is received (RX - receiving data)
            // BSP_LED_0 = 4 (Red, D9)
            // BSP_LED_1 = 5 (Orange, D10) ← Orange LED for RX
            // BSP_LED_2 = 22 (Green, D11)
            // BSP_LED_3 = 14 (Blue, D12)
            bsp_board_led_on(1);  // Orange LED (BSP_LED_1 = D10) - RX indicator
            test_run_info((unsigned char *)"PING received! Orange LED ON");
            
            // Keep LED on for 1 second
            nrf_delay_ms(1000);
            
            // Turn off orange LED
            bsp_board_led_off(1);
            test_run_info((unsigned char *)"Orange LED OFF");
        }
        
        // Small delay to prevent busy waiting
        nrf_delay_ms(10);
    }
    
    return 0;
}

#endif // TEST_UART_PING
