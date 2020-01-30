/**
 * \file serial.c
 * A simple software serial implementation
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <string.h>

#include "serial.h"

#ifndef SERIAL_BPS
//! Serial bitrate. Defaults to 4800, but the code will support up to
//! 9600bps with the clock at 1Mhz, and up to 57.6Kbps with the clock
//! at 8Mhz.
#define SERIAL_BPS 4800
#endif

//! Select an appropriate prescaler for the specified bitrate.
//!
//! `TIMER0` is an 8 bit timer, so we need to select a prescaler that will
//! let us achieve the correct bit timing in less than 255 compare matches.
//! We do this with a series of nested `#if` directives.
//!
//! \defgroup PRESCALER PRESCALER
//! @{
#if ((F_CPU/SERIAL_BPS) < 256)
#pragma message "prescaler: no prescaler"

//! `PRESCALER_FLAG` is used to configure the `TCCR0B` register
#define PRESCALER_FLAG 0b001

//! `PRESCALER_VAL` is the divisor selected by `PRESCALER_FLAG`. We
//! this to calculate `TICKS_PER_BIT`.
#define PRESCALER_VAL 1
#else
#if ((F_CPU/SERIAL_BPS/8) < 256)
#pragma message "prescaler: CLK/8"
#define PRESCALER_FLAG 0b010
#define PRESCALER_VAL  8
#else
#if ((F_CPU/SERIAL_BPS/64) < 256)
#pragma message "prescaler: CLK/64"
#define PRESCALER_FLAG 0b011
#define PRESCALER_VAL  64
#else
#pragma message "prescale: CLK/256"
#define PRESCALER_FLAG 0b100
#define PRESCALER_VAL  256
#endif
#endif
#endif
//! @}

#ifndef SERIAL_TXPIN
//! Use this pin for serial output.
#define SERIAL_TXPIN (1<<PORTB0)
#endif

#ifndef SERIAL_TXPORT
//! The port data register associated with `SERIAL_TXPIN`.
#define SERIAL_TXPORT PORTB
#endif

#ifndef SERIAL_TXDDR
//! The direction register (`DDRA`, `DDRB`, etc) associated with
//! `SERIAL_TXPORT`
#define SERIAL_TXDDR DDRB
#endif

//! The number of timer tickets that correspond to a single bit. We
//! load this value into the `OCR0A` register.
#define TICKS_PER_BIT (F_CPU/SERIAL_BPS/PRESCALER_VAL)

#ifdef __AVR_ATtiny84__
#define TIMSK TIMSK0
#endif

//! Data structure representing the output port.
volatile struct SERIAL_PORT {
    uint8_t data;   //!< Byte we are currently sending
    uint8_t index;  //!< Bit in `data` that we are currently sending
    uint8_t busy;   //!< 1 if we are currently sending data, 0 otherwise
} port;

#ifdef DEBUG
#include <simavr/avr/avr_mcu_section.h>

#define DEBUGPORT PORTB
#define DEBUGDDR DDRB
#define DEBUGPIN (1<<PORTB1)

const struct avr_mmcu_vcd_trace_t _mytrace[]  _MMCU_ = {
    { AVR_MCU_VCD_SYMBOL("TX"),  .mask = SERIAL_TXPIN, .what = (void*)&SERIAL_TXPORT, },
    { AVR_MCU_VCD_SYMBOL("ISR"), .mask = DEBUGPIN,     .what = (void*)&DEBUGPORT,     },
};
#endif

#ifdef SERIAL_PROVIDE_MILLIS

//! These values are used when updating the millis counter.
//! \defgroup  MILLIS MILLIS
//! @{

//! Number of milliseconds required to send each bit
#define mS_PER_BIT (1000/SERIAL_BPS)

//! Number of additional microseconds required to send each bit
#define uS_PER_BIT ((1000000/SERIAL_BPS) - (mS_PER_BIT * 1000))

//! @}

uint16_t _micros;
millis_t _millis;
#endif // SERIAL_PROVIDE_MILLIS

void serial_init() {
#ifdef DEBUG
    DEBUGDDR      |= 1<<DEBUGPIN;
#endif

    SERIAL_TXDDR  |= SERIAL_TXPIN;      // Set SERIAL_TXPIN as an output
    SERIAL_TXPORT |= SERIAL_TXPIN;      // Set SERIAL_TXPIN high (serial idle)
    TCCR0A         = 1<<WGM01;          // Select CTC mode
    TCCR0B         = PRESCALER_FLAG;    // Set clock prescaler
    OCR0A          = TICKS_PER_BIT;     // Set CTC target value
    sei();
}

void serial_begin() {
    TIMSK |= 1<<OCIE0A;    // Enable compare match interrupt
}

void serial_end() {
    while (port.busy);      // Wait for send to complete
    TIMSK &= ~(1<<OCIE0A);  // Disable compare match interrupt
}

void serial_putchar(char c) {
    while (port.busy);      // Wait for previous byte to complete

    port.data  = c;
    port.index = 0;
    port.busy  = 1;
}

void serial_print(char *s) {
    while (*s) serial_putchar(*s++);
}

void serial_println(char *s) {
    serial_print(s);
    serial_putchar('\r');
    serial_putchar('\n');
}

#ifdef SERIAL_PROVIDE_MILLIS
void delay(millis_t ms) {
    millis_t t_start = millis();
    while(millis() - t_start < ms);
}

millis_t millis() {
    millis_t m;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        m = _millis;
    }
    return m;
}
#endif // SERIAL_PROVIDE_MILLIS

//! Timer0 output compare match interrupt routine. This is responsible
//! for sending bits out `SERIAL_TXPIN`. If you have enabled
//! `SERIAL_PROVIDE_MILLIS`, it is also responsible for incrementing
//! the millis counter.
ISR(TIM0_COMPA_vect) {
#ifdef DEBUG
    DEBUGPORT |= 1<<DEBUGPIN;
#endif
    if (port.busy) {
        switch(port.index) {
            case 0:
                // send start bit
                SERIAL_TXPORT &= ~(SERIAL_TXPIN);
                break;
            case 9:
                // send stop bit
                SERIAL_TXPORT |= SERIAL_TXPIN;
                port.busy = 0;
                break;
            default:
                // send data bit
                if (port.data & 1) {
                    SERIAL_TXPORT |= SERIAL_TXPIN;
                } else {
                    SERIAL_TXPORT &= ~(SERIAL_TXPIN);
                }
                port.data >>= 1;
                break;
        }
        port.index++;
    }

#ifdef SERIAL_PROVIDE_MILLIS
    if ((_micros += uS_PER_BIT) >= 1000) {
        _millis++;
        _micros = 0;
    }
    _millis += mS_PER_BIT;
#endif // SERIAL_PROVIDE_MILLIS

#ifdef DEBUG
    DEBUGPORT &= ~(1<<DEBUGPIN);
#endif
}
