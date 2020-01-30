#include <Arduino.h> // needed for main()
#include "ATtinySerialOut.h"
#include <avr/pgmspace.h> // needed for PSTR()

#define VERSION_EXAMPLE "1.0"
#define EncoderClick A0 // A0 PB5, pin 1 (RESET)
unsigned char screenStatus;

void setup(void) {
    pinMode(EncoderClick, INPUT_PULLUP);
    initTXPin();

    screenStatus = 0x00;

    writeString(F("START " __FILE__ "\nVersion " VERSION_EXAMPLE " from " __DATE__ "\n"));

    uint8_t tOSCCAL = OSCCAL;

    writeString("Value of OSCCAL is:");
    writeUnsignedByteHexWithPrefix(tOSCCAL);
}

void cleanConsole() {
//    writeChar(27);
   //writeChar("[2J");
//    writeChar(27);
   //writeChar("[H");
//    writeChar(27);
}

void loop(void) {
    static uint8_t tIndex = 0;
    /*
     * Example of 3 Byte output. View in combined ASSCI / HEX View in HTerm (http://www.der-hammer.info/terminal/)
     * Otherwise use writeUnsignedByteHexWithoutPrefix or writeUnsignedByteHex
     */

    /*
     * Serial.print usage example
     */
    if (analogRead(EncoderClick) < 940) {
        // write1Start8Data1StopNoParityWithCliSei('I');
        // writeBinary(tIndex);                    // 1 Byte binary output
        // writeUnsignedByte(tIndex);              // 1-3 Byte ASCII output
        // writeUnsignedByteHexWithPrefix(tIndex); // 4 Byte output
        // writeUnsignedByteHex(tIndex);           // 2 Byte output
        // write1Start8Data1StopNoParityWithCliSei('\n');
        // tIndex++;

        // cleanConsole();
        Serial.print("Nuevo estado: ");

        if (screenStatus == 0x00) {
            screenStatus = 0xff;
            Serial.println("Blanco");
        } else {
            screenStatus = 0x00;
            Serial.println("Negro");
        }
    }

    // writeUnsignedByteHexWithPrefix(255); // 0xff
    // writeUnsignedByteHex(255); // ff
    // Serial.print((char) tIndex);
    // Serial.print(" | ");
    // Serial.print(tIndex);
    // Serial.print(" | ");
    // Serial.print(tIndex, HEX);
    // Serial.print(" | ");
    // Serial.printHex(tIndex);
    // Serial.print(" | ");
    // Serial.println(tIndex);

    // tIndex++;
    delay(500);
}
