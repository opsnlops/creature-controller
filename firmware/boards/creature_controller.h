/*
 * Minimal board header for April's Creature Workshop animatronic controllers.
 *
 * The only thing the SDK needs from us is the RP2350 package variant. The
 * firmware sets all of its own UART/I2C/SPI/LED pins, so we don't define
 * any defaults here.
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

#ifndef _BOARDS_CREATURE_CONTROLLER_H
#define _BOARDS_CREATURE_CONTROLLER_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

// RP2350B (48 GPIOs). Setting PICO_RP2350A=0 cascades into NUM_BANK0_GPIOS=48
// and PICO_PIO_USE_GPIO_BASE=1, which is what lets PIO drive pins above 31.
#define PICO_RP2350A 0

// Referenced by src/debug/blinker.c; everything else sets its own pins.
#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

#endif
