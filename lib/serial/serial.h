/**
 * \file serial.h
 *
 * A simple software serial implementation.  This will operate up to 9600bps 
 * at 1Mhz, although that doesn't leave you much time for anything else. At
 * 8Mhz you can run at 57.5Kbps.
 *
 * The code operates by configuring `TIMER0` in `CTC` mode in order to control
 * bit timing.
 *
 * ## Configuration
 *
 * You can set a bitrate by defining `SERIAL_BPS` to an appropriate
 * value.  It defaults to 4800 bps, since that will work comfortably
 * at 1Mhz.
 *
 * The code assumes you're using `PORTB0` for serial output.  If you
 * want to use another pin, you will need to set `SERIAL_TXPIN`.  If
 * you want to use a pin that is not on `PORTB`, you will also need to
 * set `SERIAL_TXDDR` and `SERIAL_TXPORT`.  E.g:
 *
 *     #define SERIAL_TXPIN (1<<PORTA0)
 *     #define SERIAL_TXDDR DDRA
 *     #define SERIAL_TXPORT PORTA
 *
 * If you define `SERIAL_PROVIDES_MILLIS`, the ISR will also take care of
 * updating a millisecond counter. You can use the `millis()` function to 
 * access it. Note that at high bps/low clock speed, enabling this feature
 * requires too much additional time and your serial output will not work.
 * This feature is disabled by default.
 */
#ifndef _serial_h
#define _serial_h

#include <stdint.h>

// Control size of millis counter
typedef uint32_t millis_t;


/**
 * Initialize software serial support. This sets up the appropriate
 * `DDR` register, `TCCR0A`, `TCCR0B`, and sets `OCR0A` to a value
 * appropriate for the selected clock speed and bitrate.
 */
void serial_init();

/** Enable timer compare interrupts */
void serial_begin();

/** Disable timer compare interrupts */
void serial_end();

/** Write a single character out the serial port */
void serial_putchar(char c);

/** Write the given string out the serial port */
void serial_print(char *s);

/** Write the given string out the serial port, follow by a CR/LF pair */
void serial_println(char *s);

#ifdef SERIAL_PROVIDE_MILLIS
/** Return current millis counter.  The millis counter starts
 * incrementing when you call `serial_begin`, and stops counting when
 * you call `serial_end`.
 */
millis_t millis();

/** Delay the given number of milliseconds */
void delay(millis_t ms);
#endif

#endif // _serial_h
