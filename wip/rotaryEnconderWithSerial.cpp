/**
 *  Bounce-Free Rotary Encoder
 *  David Johnson-Davies - www.technoblogy.com - 28th October 2017
 *  ATtiny85 @ 1 MHz (internal oscillator; BOD disabled)
 *
 *  CC BY 4.0
 *  Licensed under a Creative Commons Attribution 4.0 International license:
 *  http://creativecommons.org/licenses/by/4.0/
 */
/*
 * ATTINY13/45/85
 * ---------------------------------------------------------
 *         ATtiny13/13A/48/85
 *               ┌──┬┬──┐
 *  RESET   PB5  ┤1 └┘ 8├ Vcc
 *  TX ADC3 PB3  ┤2    7├ PB2 SCK/ADC1/T0
 *  RX ADC2 PB4  ┤3    6├ PB1 MISO/AIN1/OC0B/INT0
 *  GND          ┤4    5│ PB0 MOSI/AIN0/OC0A
 *               └──────┘
 */
/*
 * ROTARY ENCODER
 * ---------------------------------------------------------
 *
 *               ┌──────────┐
 *               │          │
 *               │          │
 *               │          │
 *               │┌┐┌┐┌┐┌┐┌┐│
 *               └┴┴┴┴┴┴┴┴┴┴┘
 *             GND + SW DATA CLK
 */


#include "Arduino.h"
#include "SoftwareSerial.h"

// Serial **********************************************
#define RX 10 // We can not pass a NULL value here...
#define TX 0 // PB5, Pin 5
SoftwareSerial MySerial(RX, TX);

// Timming **********************************************
const int DELAY = 100;

// Rotary encoder **********************************************
const int EncoderA = 2; // PB2, pin 7 (INT0) (DATA in Rotary Encoder)
const int EncoderB = 1; // PB1, pin 6 (CLK in Rotary Encoder)
const int EncoderClick = A0; // A0 PB5, pin 1 (RESET)
volatile int a0;
volatile int c0;
volatile int Count = 0;
volatile int OldCount = 0;

// Called when encoder value changes
void ChangeValue (bool Up) {
    Count = max(min((Count + (Up ? 1 : -1)), 1000), 0);
}

// Pin cha nge interrupt service routine
void changeRotary() {
    int a = PINB>>EncoderA & 1;
    int b = PINB>>EncoderB & 1;

    // A changed
    if (a != a0) {
        a0 = a;
        if (b != c0) {
            c0 = b;
            ChangeValue(a == b);
        }
    }
}

void setup() {
    pinMode(EncoderA, INPUT_PULLUP);
    pinMode(EncoderB, INPUT_PULLUP);
    pinMode(EncoderClick, INPUT_PULLUP);

    attachInterrupt(0, changeRotary, CHANGE);

    MySerial.begin( 4800 );
    MySerial.println("Initializing MySerial...");
}

void loop() {
    if (OldCount != Count) {
        MySerial.print("Turning: ");
        MySerial.println(Count);
        OldCount = Count;
    }

    // Reset count on Rotary Click
    if (analogRead(EncoderClick) < 940) {
        Count = 0;
    }

    delay(DELAY);
}
