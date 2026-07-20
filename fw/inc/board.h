/* board.h — pin map + clock config for LMX259X-PLL Mixer Module (STM32F103C8T6).
 * All pins verified by firmware reverse-engineering + physical board inspection.
 * See OpenContext lmx2594-pll/research/hardware-pinout.md. */
#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx.h"

/* ---- Clock: 8 MHz HSE -> PLL x9 -> 72 MHz SYSCLK (PCLK2=72M, PCLK1=36M) ---- */
#define HSE_HZ        8000000u
#define SYSCLK_HZ     72000000u
#define PCLK1_HZ      36000000u
#define PCLK2_HZ      72000000u

/* ---- LMX2594 bit-bang SPI (GPIOB) + CE (GPIOA). Reverse: FUN_080069F4. ---- */
#define LMX_SPI_PORT  GPIOB
#define LMX_CS_PIN    1u    /* PB1  = CSB / LE  (latch enable) */
#define LMX_DATA_PIN  11u   /* PB11 = SDI (MOSI) */
#define LMX_CLK_PIN   12u   /* PB12 = SCK */
#define LMX_CE_PORT   GPIOA
#define LMX_CE_PIN    3u    /* PA3  = CE (chip enable, active high) */
#define LMX_MUX_PORT  GPIOB
#define LMX_MUX_PIN   10u   /* PB10 = MUXout (lock detect / SPI readback SDO).
                            * pin21 LQFP48, wired to RF-module MUXout header.
                            * Stock firmware wrongly used this as USART3-TX. */
/* Aux LMX2594 header signals — wired to MCU but unused for LO operation.
 * Documented for completeness (ramp/FSK, JESD sync — not needed here). */
#define LMX_RAMPDIR_PORT GPIOA
#define LMX_RAMPDIR_PIN  4u    /* PA4  = RampDir (pin14). Unused. */
#define LMX_SYSREF_PORT  GPIOB
#define LMX_SYSREF_PIN   0u    /* PB0  = SysRefReq (pin18). Unused. */
#define LMX_SYNC_PORT    GPIOB
#define LMX_SYNC_PIN     13u   /* PB13 = SYNC (pin44). Unused. */
#define LMX_RAMPCLK_PORT GPIOA
#define LMX_RAMPCLK_PIN  5u    /* PA5  = RampCLK (pin15). Unused. */
/* Full RF-module header pin map complete: all 9 signals mapped. */

/* ---- LCD ST7789 240x240 bit-bang (GPIOB). Reverse: FUN_0800194C etc. ---- */
#define LCD_PORT      GPIOB
#define LCD_SCLK_PIN  5u    /* PB5 */
#define LCD_MOSI_PIN  6u    /* PB6 */
#define LCD_RST_PIN   7u    /* PB7 */
#define LCD_DC_PIN    8u    /* PB8 */
/* LCD CS: reverse used DAT_080028F4 on GPIOB; pin TBD on new board bring-up. */

/* ---- 5 push buttons, active-low, pull-up. Reverse: FUN_0800266C. ---- */
#define BTN_LEFT_PORT   GPIOA
#define BTN_LEFT_PIN    8u   /* PA8  -> code 1 */
#define BTN_DOWN_PORT   GPIOA
#define BTN_DOWN_PIN    1u   /* PA1  -> code 2 */
#define BTN_RIGHT_PORT  GPIOA
#define BTN_RIGHT_PIN   0u   /* PA0  -> code 3 */
#define BTN_UP_PORT     GPIOA
#define BTN_UP_PIN      2u   /* PA2  -> code 4 */
#define BTN_CENTER_PORT GPIOB
#define BTN_CENTER_PIN  15u  /* PB15 -> code 5 */

/* ---- TTL-UART command channel = USART1 on PA9/PA10 (physical side header).
 * NOTE: stock firmware wrongly used USART3/PB10-PB11; the real header is USART1.
 * See research/uart-channel.md. ---- */
#define CLI_USART       USART1
#define CLI_TX_PIN      9u    /* PA9  = TX  (AF push-pull) */
#define CLI_RX_PIN      10u   /* PA10 = RX  (input floating) */
#define CLI_BAUD        115200u

/* ---- LMX2594 reference / PFD. 10 MHz TCXO, internal MULT=5 -> 50 MHz PFD. ---- */
#define LMX_PFD_KHZ     50000u    /* phase-detector freq in kHz (reverse DAT_08006298) */
#define LMX_VCO_MIN_KHZ 7500000u  /* 7.5 GHz */
#define LMX_VCO_MAX_KHZ 15000000u /* 15  GHz */
#define LMX_FOUT_MIN_KHZ 20000u   /* 20 MHz */
#define LMX_FOUT_MAX_KHZ 15000000u/* 15 GHz — VCO max (direct). 15.5G on the label is
                                   * unreachable: LMX2594 VCO is 7.5-15 GHz, output can't
                                   * exceed VCO. Stock firmware had the same physical limit. */

#endif /* BOARD_H */
