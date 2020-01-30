#include <stdio.h>
#include <util/delay.h>
#include "serial.h"
#include <Arduino.h>

#define EncoderClick A0 // A0 PB5, pin 1 (RESET)
unsigned char screenStatus;

void cleanConsole() {
//   serial_putchar(27);
//   serial_putchar("[2J");
//   serial_putchar(27);
//   serial_putchar("[H");
//   serial_putchar(27);
}

void setup() {
#ifdef SERIAL_PROVIDE_MILLIS
    char buf[20];
#endif
    screenStatus = 0x00;
    pinMode(EncoderClick, INPUT_PULLUP);
    serial_init();
}

void loop() {
    if (analogRead(EncoderClick) < 940) {
        serial_begin();
        cleanConsole();

        serial_print("Nuevo estado: ");
        if (screenStatus == 0x00) {
            screenStatus = 0xff;
            serial_println("Blanco");
        } else {
            screenStatus = 0x00;
            serial_println("Negro");
        }

        serial_end();
    }
}

