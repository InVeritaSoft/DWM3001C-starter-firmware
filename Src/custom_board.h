/**
 * @file      custom board.h
 * 
 * @brief     pin mapping description corresponding to DWM30001_A1 module
 *
 * @author    Decawave Applications
 *
 * @attention Copyright (c) 2021 - 2022, Qorvo US, Inc.
 * All rights reserved
 * Redistribution and use in source and binary forms, with or without modification,
 *  are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 * 3. You may only use this software, with or without any modification, with an
 *  integrated circuit developed by Qorvo US, Inc. or any of its affiliates
 *  (collectively, "Qorvo"), or any module that contains such integrated circuit.
 * 4. You may not reverse engineer, disassemble, decompile, decode, adapt, or
 *  otherwise attempt to derive or gain access to the source code to any software
 *  distributed under this license in binary or object code form, in whole or in
 *  part.
 * 5. You may not use any Qorvo name, trademarks, service marks, trade dress,
 *  logos, trade names, or other symbols or insignia identifying the source of
 *  Qorvo's products or services, or the names of any of Qorvo's developers to
 *  endorse or promote products derived from this software without specific prior
 *  written permission from Qorvo US, Inc. You must not call products derived from
 *  this software "Qorvo", you must not have "Qorvo" appear in their name, without
 *  the prior permission from Qorvo US, Inc.
 * 6. Qorvo may publish revised or new version of this license from time to time.
 *  No one other than Qorvo US, Inc. has the right to modify the terms applicable
 *  to the software provided under this license.
 * THIS SOFTWARE IS PROVIDED BY QORVO US, INC. "AS IS" AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. NEITHER
 *  QORVO, NOR ANY PERSON ASSOCIATED WITH QORVO MAKES ANY WARRANTY OR
 *  REPRESENTATION WITH RESPECT TO THE COMPLETENESS, SECURITY, RELIABILITY, OR
 *  ACCURACY OF THE SOFTWARE, THAT IT IS ERROR FREE OR THAT ANY DEFECTS WILL BE
 *  CORRECTED, OR THAT THE SOFTWARE WILL OTHERWISE MEET YOUR NEEDS OR EXPECTATIONS.
 * IN NO EVENT SHALL QORVO OR ANYBODY ASSOCIATED WITH QORVO BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 *
 */

#ifndef BOARD_DWM3001_A1_H
#define BOARD_DWM3001_A1_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"
#include "nrf_spim.h"

// LEDs definitions for PCA10056
#define LEDS_NUMBER    4

//If AOA chip is populated on the PCB without second antenna, this shall be set to (1)
#define AOA_CHIP_ON_NON_AOA_PCB (0)

#define LED_1          NRF_GPIO_PIN_MAP(0,4)  //D9 on the schematics
#define LED_2          NRF_GPIO_PIN_MAP(0,5)  //D10 on the schematics
#define LED_3          NRF_GPIO_PIN_MAP(0,22) //D11 on the schematics
#define LED_4          NRF_GPIO_PIN_MAP(0,14) //D12 on the schematics
#define LED_START      LED_1
#define LED_STOP       LED_4

#define LEDS_ACTIVE_STATE 0

#define LEDS_LIST { LED_1, LED_2, LED_3, LED_4 }

#define LEDS_INV_MASK  LEDS_MASK

#define BSP_LED_0      4
#define BSP_LED_1      5
#define BSP_LED_2      22
#define BSP_LED_3      14

#define BUTTONS_NUMBER 1

#define BUTTON_1       2

#define BUTTON_PULL    NRF_GPIO_PIN_PULLUP

#define BUTTONS_ACTIVE_STATE 0

#define BUTTONS_LIST { BUTTON_1}

#define BSP_BUTTON_0   BUTTON_1

// UART pin configuration - CHANGED to avoid LED conflict
// PROBLEM: GPIO 14 (P0.14) conflicts with LED_4 (D12 LED) - causes "dead pins"
// SOLUTION: Use GPIO 15 for both TX and RX (confirmed working in testing)
// 
// From J10 pin mapping:
// - J10 Pin 8 = GPIO14 (TXD0) - CONFLICTS with LED_4 ❌
// - J10 Pin 10 = GPIO15 (RXD0) - Works for RX ✅
// - J10 Pin 15 = GPIO15 (GPIO_RPI) - Same as Pin 10, also works ✅
//
// Testing showed GPIO 15 works for both TX and RX
// We'll use GPIO 15 for TX (since it's confirmed working) and keep RX on GPIO 15
// NOTE: This means TX and RX share the same pin, which is unusual but works if
// we're careful about timing. Alternatively, we can try GPIO 27 for TX (J10 Pin 13)
// which was confirmed to work for TX in testing.
//
// BEST OPTION: Use GPIO 15 for RX (confirmed) and GPIO 27 for TX (J10 Pin 19, confirmed working)
#define RX_PIN_NUMBER  15  // GPIO 15 (P0.15) - J10 Pin 10 or Pin 15 (RXD0) - Confirmed working for RX
#define TX_PIN_NUMBER  27  // GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX) - Confirmed working for TX
#define CTS_PIN_NUMBER (-1)
#define RTS_PIN_NUMBER (-1)
#define HWFC           false

// Arduino board mappings
#define ARDUINO_13_PIN              NRF_GPIO_PIN_MAP(0,  3)  // used as DW3000_CLK_Pin
#define ARDUINO_12_PIN              NRF_GPIO_PIN_MAP(0, 29)  // used as DW3000_MISO_Pin
#define ARDUINO_11_PIN              NRF_GPIO_PIN_MAP(0,  8)  // used as DW3000_MOSI_Pin
#define ARDUINO_10_PIN              NRF_GPIO_PIN_MAP(1,  6)  // used as DW3000_CS_Pin
#define ARDUINO_9_PIN               NRF_GPIO_PIN_MAP(1, 19)  // used as DW3000_WKUP_Pin
#define ARDUINO_8_PIN               NRF_GPIO_PIN_MAP(1,  2)  // used as DW3000_IRQ_Pin
#define ARDUINO_7_PIN               NRF_GPIO_PIN_MAP(0, 25)  // used as DW3000_RST_Pin

#define DW3000_RST_Pin      ARDUINO_7_PIN
#define DW3000_IRQ_Pin      ARDUINO_8_PIN
#define DW3000_WUP_Pin      ARDUINO_9_PIN

// SPI defs
#define DW3000_CS_Pin       ARDUINO_10_PIN
#define DW3000_CLK_Pin      ARDUINO_13_PIN  // DWM3000 shield SPIM1 sck connected to DW1000
#define DW3000_MOSI_Pin     ARDUINO_11_PIN  // DWM3000 shield SPIM1 mosi connected to DW1000
#define DW3000_MISO_Pin     ARDUINO_12_PIN  // DWM3000 shield SPIM1 miso connected to DW1000
#define DW3000_SPI_IRQ_PRIORITY APP_IRQ_PRIORITY_LOW

// UART symbolic constants
// CHANGED: Using GPIO 15 for RX and GPIO 27 for TX to avoid LED conflict
// - TX: GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX) - Confirmed working for TX (no LED conflict)
// - RX: GPIO 15 (P0.15) - J10 Pin 10/15 (RXD0) - Confirmed working for RX (no LED conflict)
#define UART_0_TX_PIN       TX_PIN_NUMBER           // GPIO 27 (P0.27) - J10 Pin 19 (GPIO27_PIN19_TX) - Confirmed working
#define UART_0_RX_PIN       RX_PIN_NUMBER           // GPIO 15 (P0.15) - J10 Pin 10/15 - Confirmed working
#define UART_PIN_DISCONNECTED  0xFFFFFFFF           // Disconnected pin value
#define DW3000_RTS_PIN_NUM      UART_PIN_DISCONNECTED
#define DW3000_CTS_PIN_NUM      UART_PIN_DISCONNECTED

// RS-485 transceiver control pins
// DE (Driver Enable): HIGH = transmit mode, LOW = receive mode
// RE (Receiver Enable): LOW = receive mode, HIGH = disabled (often tied to DE via inverter)
// For MAX485/MAX3485: DE and RE are typically tied together (active-high for TX, active-low for RX)
// Using GPIO 13 (P0.13) for RS-485 DE/RE control (available on J10 Pin 6)
// DISABLED: Module may have automatic direction control or DE/RE internally tied
// #define RS485_DE_PIN        NRF_GPIO_PIN_MAP(0, 13)  // GPIO 13 (P0.13) - RS-485 Driver Enable

#define LED_ERROR BSP_LED_0

#define DW3000_MIN_SPI_FREQ (NRF_SPIM_FREQ_4M)
#define DW3000_MAX_SPI_FREQ (NRF_SPIM_FREQ_32M)
#define DW3000_SPI_IDX	(3)

#ifdef __cplusplus
}
#endif

#endif // PCA10100_H
