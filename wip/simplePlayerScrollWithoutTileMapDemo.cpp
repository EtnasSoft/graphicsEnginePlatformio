// #define DEBUG // To see serial output with 115200 baud at P2 -

#include <Arduino.h>
#include <avr/interrupt.h>

#ifdef DEBUG
#include "ATtinySerialOut.h"
#endif

// Timming **********************************************
const int DELAY = 100;

// OLED Screen config **********************************************
// Changing defaults for avoid conflicts with interruptions
#define SSD1306_SCL PORTB4 // SCL, Pin 3
#define SSD1306_SDA PORTB3 // SDA, Pin 2
#define SSD1306_SA 0x3C // Slave Address
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Rotary encoder **********************************************
const int EncoderA = 2; // PB2, pin 7 (INT0)
const int EncoderB = 1; // PB1, pin 6
const int EncoderClick = A0; // A0 PB5, pin 1 (RESET)
volatile int a0;
volatile int c0;

// Player and Game vars **********************************************
int lives; // Lives in the game - this can go negative
unsigned int player = 15; // Position of the player in the listObjects
bool playerAction = 0;    // Captures when the 'click' button is pressed

volatile int playerDirection = 0; // Captures the current direction from the rotary encoder
volatile int oldPlayerDirection = 0; // Stores the last direction from the rotary encoder

volatile int backgroundPosX = 0; // Initial move of x-axis background from rigth to left
volatile int backgroundPosY = 0; // Initial move of y-axis background from bottom to top
bool backgroundReset = 0; // Return to default background move

int alienSpeed = 0; // Speed of the aliens moves
int level; // Game level - incremented every time you clear the screen

// Prototypes **********************************************
void gameLoop(void);

typedef struct tag_gfx_object {
  byte x;
  byte y;
  byte bType; // type and index (high bit set = 16x16, else 8x8), up to 128
              // unique sprites
} GFX_OBJECT;

static byte bPlayfield[18 * 10]; // 18x10 (16x8 + edges) scrolling playfield
static int iScrollX, iScrollY;

const int numberOfSprites = 1;

// Objects and sprites **********************************************
// active objects (can be any number which fits in RAM)
static GFX_OBJECT object_list[numberOfSprites];

// 8x8px sprite: phantom (Pac Man)
// 8 bytes of mask followed by 8 bytes of pattern
const byte ucSprites[] PROGMEM = {
  0x7C, 0xF6, 0x66, 0xFF, 0x7F, 0xF6, 0x66, 0xFC,
  0x7C, 0xF6, 0x66, 0xFF, 0x7F, 0xF6, 0x66, 0xFC
};


// 32 bytes of mask followed by 32 bytes of pattern
const byte ucBigSprites[] PROGMEM = {
  0xff, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x03, 0xff,
  0xff, 0xc0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xc0, 0xff,

  0x00, 0x00, 0x00, 0xc0, 0x20, 0x10, 0x48, 0x08,
  0x08, 0x48, 0x10, 0x20, 0xc0, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x04, 0x08, 0x12, 0x14,
  0x14, 0x12, 0x08, 0x04, 0x03, 0x00, 0x00, 0x00,
};

const byte ucTiles[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // SPACE
    0x00, 0x00, 0x06, 0x5f, 0x5f, 0x06, 0x00, 0x00, // !
    0x00, 0x07, 0x07, 0x00, 0x07, 0x07, 0x00, 0x00, // "
    0x14, 0x7f, 0x7f, 0x14, 0x7f, 0x7f, 0x14, 0x00, // #
    0x24, 0x2e, 0x2a, 0x6b, 0x6b, 0x3a, 0x12, 0x00, // $
    0x46, 0x66, 0x30, 0x18, 0x0c, 0x66, 0x62, 0x00, // %
    0x30, 0x7a, 0x4f, 0x5d, 0x37, 0x7a, 0x48, 0x00, // &
    0x00, 0x04, 0x07, 0x03, 0x00, 0x00, 0x00, 0x00, // '
    0x00, 0x1c, 0x3e, 0x63, 0x41, 0x00, 0x00, 0x00, // (
    0x00, 0x41, 0x63, 0x3e, 0x1c, 0x00, 0x00, 0x00, // )
    0x08, 0x2a, 0x3e, 0x1c, 0x1c, 0x3e, 0x2a, 0x08, // *
    0x00, 0x08, 0x08, 0x3e, 0x3e, 0x08, 0x08, 0x00, // +
    0x00, 0x00, 0x80, 0xe0, 0x60, 0x00, 0x00, 0x00, // ,
    0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, // -
    0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x00, 0x00, // .
    0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x01, 0x00, // /  (15)
    0x3e, 0x7f, 0x59, 0x4d, 0x47, 0x7f, 0x3e, 0x00, // 0
    0x40, 0x42, 0x7f, 0x7f, 0x40, 0x40, 0x00, 0x00, // 1
    0x62, 0x73, 0x59, 0x49, 0x6f, 0x66, 0x00, 0x00, // 2
    0x22, 0x63, 0x49, 0x49, 0x7f, 0x36, 0x00, 0x00, // 3
    0x18, 0x1c, 0x16, 0x53, 0x7f, 0x7f, 0x50, 0x00, // 4
    0x27, 0x67, 0x45, 0x45, 0x7d, 0x39, 0x00, 0x00, // 5
    0x3c, 0x7e, 0x4b, 0x49, 0x79, 0x30, 0x00, 0x00, // 6
    0x03, 0x03, 0x71, 0x79, 0x0f, 0x07, 0x00, 0x00, // 7
    0x36, 0x7f, 0x49, 0x49, 0x7f, 0x36, 0x00, 0x00, // 8
    0x06, 0x4f, 0x49, 0x69, 0x3f, 0x1e, 0x00, 0x00, // 9
    0x00, 0x00, 0x00, 0x66, 0x66, 0x00, 0x00, 0x00, // :
    0x00, 0x00, 0x80, 0xe6, 0x66, 0x00, 0x00, 0x00, // ;
    0x08, 0x1c, 0x36, 0x63, 0x41, 0x00, 0x00, 0x00, // <
    0x00, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, // =
    0x00, 0x41, 0x63, 0x36, 0x1c, 0x08, 0x00, 0x00, // >  (30)
    0x00, 0x02, 0x03, 0x59, 0x5d, 0x07, 0x02, 0x00, // ?
    0x3e, 0x7f, 0x41, 0x5d, 0x5d, 0x5f, 0x0e, 0x00, // @
    0x7c, 0x7e, 0x13, 0x13, 0x7e, 0x7c, 0x00, 0x00, // A
    0x41, 0x7f, 0x7f, 0x49, 0x49, 0x7f, 0x36, 0x00, // B
    0x1c, 0x3e, 0x63, 0x41, 0x41, 0x63, 0x22, 0x00, // C
    0x41, 0x7f, 0x7f, 0x41, 0x63, 0x3e, 0x1c, 0x00, // D
    0x41, 0x7f, 0x7f, 0x49, 0x5d, 0x41, 0x63, 0x00, // E
    0x41, 0x7f, 0x7f, 0x49, 0x1d, 0x01, 0x03, 0x00, // F
    0x1c, 0x3e, 0x63, 0x41, 0x51, 0x33, 0x72, 0x00, // G
    0x7f, 0x7f, 0x08, 0x08, 0x7f, 0x7f, 0x00, 0x00, // H
    0x00, 0x41, 0x7f, 0x7f, 0x41, 0x00, 0x00, 0x00, // I
    0x30, 0x70, 0x40, 0x41, 0x7f, 0x3f, 0x01, 0x00, // J
    0x41, 0x7f, 0x7f, 0x08, 0x1c, 0x77, 0x63, 0x00, // K
    0x41, 0x7f, 0x7f, 0x41, 0x40, 0x60, 0x70, 0x00, // L
    0x7f, 0x7f, 0x0e, 0x1c, 0x0e, 0x7f, 0x7f, 0x00, // M  (45)
    0x7f, 0x7f, 0x06, 0x0c, 0x18, 0x7f, 0x7f, 0x00, // N
    0x1c, 0x3e, 0x63, 0x41, 0x63, 0x3e, 0x1c, 0x00, // O
    0x41, 0x7f, 0x7f, 0x49, 0x09, 0x0f, 0x06, 0x00, // P
    0x1e, 0x3f, 0x21, 0x31, 0x61, 0x7f, 0x5e, 0x00, // Q
    0x41, 0x7f, 0x7f, 0x09, 0x19, 0x7f, 0x66, 0x00, // R
    0x26, 0x6f, 0x4d, 0x49, 0x59, 0x73, 0x32, 0x00, // S
    0x03, 0x41, 0x7f, 0x7f, 0x41, 0x03, 0x00, 0x00, // T
    0x7f, 0x7f, 0x40, 0x40, 0x7f, 0x7f, 0x00, 0x00, // U
    0x1f, 0x3f, 0x60, 0x60, 0x3f, 0x1f, 0x00, 0x00, // V
    0x3f, 0x7f, 0x60, 0x30, 0x60, 0x7f, 0x3f, 0x00, // W
    0x63, 0x77, 0x1c, 0x08, 0x1c, 0x77, 0x63, 0x00, // X
    0x07, 0x4f, 0x78, 0x78, 0x4f, 0x07, 0x00, 0x00, // Y
    0x47, 0x63, 0x71, 0x59, 0x4d, 0x67, 0x73, 0x00, // Z
    0x00, 0x7f, 0x7f, 0x41, 0x41, 0x00, 0x00, 0x00, // [
    0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, // \  (60)
    0x00, 0x41, 0x41, 0x7f, 0x7f, 0x00, 0x00, 0x00, // ]
    0x08, 0x0c, 0x06, 0x03, 0x06, 0x0c, 0x08, 0x00, // ^
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, // _
    0x00, 0x00, 0x03, 0x07, 0x04, 0x00, 0x00, 0x00, // `
    0x20, 0x74, 0x54, 0x54, 0x3c, 0x78, 0x40, 0x00, // a
    0x41, 0x7f, 0x3f, 0x48, 0x48, 0x78, 0x30, 0x00, // b
    0x38, 0x7c, 0x44, 0x44, 0x6c, 0x28, 0x00, 0x00, // c
    0x30, 0x78, 0x48, 0x49, 0x3f, 0x7f, 0x40, 0x00, // d
    0x38, 0x7c, 0x54, 0x54, 0x5c, 0x18, 0x00, 0x00, // e
    0x48, 0x7e, 0x7f, 0x49, 0x03, 0x06, 0x00, 0x00, // f
    0x98, 0xbc, 0xa4, 0xa4, 0xf8, 0x7c, 0x04, 0x00, // g
    0x41, 0x7f, 0x7f, 0x08, 0x04, 0x7c, 0x78, 0x00, // h
    0x00, 0x44, 0x7d, 0x7d, 0x40, 0x00, 0x00, 0x00, // i
    0x60, 0xe0, 0x80, 0x84, 0xfd, 0x7d, 0x00, 0x00, // j
    0x41, 0x7f, 0x7f, 0x10, 0x38, 0x6c, 0x44, 0x00, // k
    0x00, 0x41, 0x7f, 0x7f, 0x40, 0x00, 0x00, 0x00, // l
    0x7c, 0x7c, 0x18, 0x78, 0x1c, 0x7c, 0x78, 0x00, // m
    0x7c, 0x78, 0x04, 0x04, 0x7c, 0x78, 0x00, 0x00, // n
    0x38, 0x7c, 0x44, 0x44, 0x7c, 0x38, 0x00, 0x00, // o
    0x84, 0xfc, 0xf8, 0xa4, 0x24, 0x3c, 0x18, 0x00, // p
    0x18, 0x3c, 0x24, 0xa4, 0xf8, 0xfc, 0x84, 0x00, // q
    0x44, 0x7c, 0x78, 0x4c, 0x04, 0x0c, 0x18, 0x00, // r
    0x48, 0x5c, 0x54, 0x74, 0x64, 0x24, 0x00, 0x00, // s
    0x04, 0x04, 0x3e, 0x7f, 0x44, 0x24, 0x00, 0x00, // t
    0x3c, 0x7c, 0x40, 0x40, 0x3c, 0x7c, 0x40, 0x00, // u
    0x1c, 0x3c, 0x60, 0x60, 0x3c, 0x1c, 0x00, 0x00, // v
    0x3c, 0x7c, 0x60, 0x30, 0x60, 0x7c, 0x3c, 0x00, // w
    0x44, 0x6c, 0x38, 0x10, 0x38, 0x6c, 0x44, 0x00, // x
    0x9c, 0xbc, 0xa0, 0xa0, 0xfc, 0x7c, 0x00, 0x00, // y
    0x4c, 0x64, 0x74, 0x5c, 0x4c, 0x64, 0x00, 0x00, // z
    0x08, 0x08, 0x3e, 0x77, 0x41, 0x41, 0x00, 0x00, // {
    0x00, 0x00, 0x00, 0x77, 0x77, 0x00, 0x00, 0x00, // |
    0x41, 0x41, 0x77, 0x3e, 0x08, 0x08, 0x00, 0x00, // }
    0x02, 0x03, 0x01, 0x03, 0x02, 0x03, 0x01, 0x00, // ~
    0x70, 0x78, 0x4c, 0x46, 0x4c, 0x78, 0x70, 0x00  // DEL (delete)
};

// some globals
static int iScreenOffset; // current write offset of screen data
static void oledWriteCommand(unsigned char c);

#define DIRECT_PORT
#define I2CPORT PORTB
#define SAFE_DELAY 0

// A bit set to 1 in the DDR is an output, 0 is an INPUT
#define I2CDDR DDRB

// Transmit a byte and ack bit
static inline void i2cByteOut(byte b) {
  byte i;
  byte bOld = I2CPORT & ~((1 << SSD1306_SDA) | (1 << SSD1306_SCL));
  for (i = 0; i < 8; i++) {
    bOld &= ~(1 << SSD1306_SDA);
    if (b & 0x80)
      bOld |= (1 << SSD1306_SDA);
    I2CPORT = bOld;
    delayMicroseconds(SAFE_DELAY);
    I2CPORT |= (1 << SSD1306_SCL);
    delayMicroseconds(SAFE_DELAY);
    I2CPORT = bOld;
    b <<= 1;
  }

  I2CPORT = bOld & ~(1 << SSD1306_SDA); // set data low
  delayMicroseconds(SAFE_DELAY);
  I2CPORT |= (1 << SSD1306_SCL); // toggle clock
  delayMicroseconds(SAFE_DELAY);
  I2CPORT = bOld;
}

void i2cBegin(byte addr) {
  I2CPORT |= ((1 << SSD1306_SDA) + (1 << SSD1306_SCL));
  I2CDDR |= ((1 << SSD1306_SDA) + (1 << SSD1306_SCL));
  I2CPORT &= ~(1 << SSD1306_SDA); // data line low first
  delayMicroseconds(SAFE_DELAY);
  I2CPORT &= ~(1 << SSD1306_SCL); // then clock line low is a START signal
  i2cByteOut(addr << 1);     // send the slave address
}

void i2cWrite(byte *pData, byte bLen) {
  byte i, b;
  byte bOld = I2CPORT & ~((1 << SSD1306_SDA) | (1 << SSD1306_SCL));

  while (bLen--) {
    b = *pData++;
    //     i2cByteOut(b);
    //#ifdef FUTURE

    // special case can save time
    if (b == 0 || b == 0xff) {
      bOld &= ~(1 << SSD1306_SDA);
      if (b & 0x80)
        bOld |= (1 << SSD1306_SDA);
      I2CPORT = bOld;
      for (i = 0; i < 8; i++) {
        I2CPORT |= (1 << SSD1306_SCL); // just toggle SCL, SDA stays the same
        delayMicroseconds(SAFE_DELAY);
        I2CPORT = bOld;
      }
    } else {
      // normal byte needs every bit tested
      for (i = 0; i < 8; i++) {
        bOld &= ~(1 << SSD1306_SDA);
        if (b & 0x80)
          bOld |= (1 << SSD1306_SDA);

        I2CPORT = bOld;
        delayMicroseconds(SAFE_DELAY);
        I2CPORT |= (1 << SSD1306_SCL);
        delayMicroseconds(SAFE_DELAY);
        I2CPORT = bOld;
        b <<= 1;
      }
    }

    // ACK bit seems to need to be set to 0, but SDA line doesn't need to be tri-state
    I2CPORT &= ~(1 << SSD1306_SDA);
    I2CPORT |= (1 << SSD1306_SCL); // toggle clock
    delayMicroseconds(SAFE_DELAY);
    I2CPORT &= ~(1 << SSD1306_SCL);
  }
}

// Send I2C STOP condition
void i2cEnd() {
  I2CPORT &= ~(1 << SSD1306_SDA);
  I2CPORT |= (1 << SSD1306_SCL);
  I2CPORT |= (1 << SSD1306_SDA);
  I2CDDR &= ((1 << SSD1306_SDA) | (1 << SSD1306_SCL)); // let the lines float (tri-state)
}

// Wrapper function to write I2C data on Arduino
static void I2CWrite(unsigned char *pData, int iLen) {
  i2cBegin(SSD1306_SA);
  i2cWrite(pData, iLen);
  i2cEnd();
}

static void I2CWriteData(unsigned char *pData, int iLen) {
  i2cBegin(SSD1306_SA);
  i2cByteOut(0x40);
  i2cWrite(pData, iLen);
  i2cEnd();
}

// Initializes the OLED controller into "page mode"
void oledInit(int bFlip, int bInvert) {
  unsigned char uc[4];
  unsigned char oled_initbuf[] = {
      0x00, 0xae, 0xa8, 0x3f, 0xd3, 0x00, 0x40, 0xa1, 0xc8, 0xda, 0x12,
      0x81, 0xff, 0xa4, 0xa6, 0xd5, 0x80, 0x8d, 0x14, 0xaf, 0x20, 0x02};

  I2CDDR &= ~(1 << SSD1306_SDA);
  I2CDDR &= ~(1 << SSD1306_SCL); // let them float high
  I2CPORT |= (1 << SSD1306_SDA); // set both lines to get pulled up
  I2CPORT |= (1 << SSD1306_SCL);

  I2CWrite(oled_initbuf, sizeof(oled_initbuf));
  if (bInvert) {
    uc[0] = 0;    // command
    uc[1] = 0xa7; // invert command
    I2CWrite(uc, 2);
  }

  // rotate display 180
  if (bFlip) {
    uc[0] = 0; // command
    uc[1] = 0xa0;
    I2CWrite(uc, 2);
    uc[1] = 0xc0;
    I2CWrite(uc, 2);
  }
}

// Sends a command to turn off the OLED display
void oledShutdown() {
  oledWriteCommand(0xaE); // turn off OLED
}

// Send a single byte command to the OLED controller
static void oledWriteCommand(unsigned char c) {
  unsigned char buf[2];

  buf[0] = 0x00; // command introducer
  buf[1] = c;
  I2CWrite(buf, 2);
}

static void oledWriteCommand2(unsigned char c, unsigned char d) {
  unsigned char buf[3];

  buf[0] = 0x00;
  buf[1] = c;
  buf[2] = d;
  I2CWrite(buf, 3);
}

// Sets the brightness (0=off, 255=brightest)
void oledSetContrast(unsigned char ucContrast) {
  oledWriteCommand2(0x81, ucContrast);
}

// Send commands to position the "cursor" (aka memory write address)
// to the given row and column
static void oledSetPosition(int x, int y) {
  oledWriteCommand(0xb0 | y);                // go to page Y
  oledWriteCommand(0x00 | (x & 0xf));        // // lower col addr
  oledWriteCommand(0x10 | ((x >> 4) & 0xf)); // upper col addr
  iScreenOffset = (y * SCREEN_WIDTH) + x;
}

// Fill the frame buffer with a byte pattern
// e.g. all off (0x00) or all on (0xff)
void oledFill(unsigned char ucData) {
  int x, y;
  unsigned char temp[16];

  memset(temp, ucData, 16);
  for (y = 0; y < 8; y++) {
    oledSetPosition(0, y); // set to (0,Y)
    for (x = 0; x < 8; x++) {
      I2CWriteData(temp, 16);
    }
  }
}

// Draw 1 character space that's vertically shifted
void DrawShiftedChar(byte *s1, byte *s2, byte *d, byte bXOff, byte bYOff) {
  byte c, c2, z;

  for (z = 0; z < (8 - bXOff); z++) {
    c = pgm_read_byte(s1++);
    c >>= bYOff; // shift over
    c2 = pgm_read_byte(s2++);
    c2 <<= (8 - bYOff);
    *d++ = (c | c2);
  }
}

// Draw the sprites visible on the current line
void DrawSprites(byte y, byte *pBuf, GFX_OBJECT *pList, byte bCount) {
  byte i, x, bSize, bSprite, *s, *d;
  byte cOld, cNew, mask, bYOff, bWidth;

  GFX_OBJECT *pObject;
  for (i = 0; i < bCount; i++) {
    pObject = &pList[i];
    bSprite = pObject->bType;          // index
    bSize = (bSprite & 0x80) ? 16 : 8; // big or small sprite
    // see if it's visible
    if (pObject->y >= y + 8) // past bottom
      continue;
    if (pObject->y + bSize <= y) // above top
      continue;
    if (pObject->x >= 128) // off right edge
      continue;
    // It's visible on this line; draw it
    bSprite &= 0x7f;       // sprite index
    d = &pBuf[pObject->x]; // destination pointer
    if (bSize == 16) {
      s = (byte *)&ucBigSprites[bSprite * 64];
      if (pObject->y + 8 <= y) // special case - only bottom half drawn
        s += 16;
      bYOff = pObject->y & 7;
      bWidth = 16;
      if (128 - pObject->x < 16)
        bWidth = 128 - pObject->x;
      // 4 possible cases:
      // byte aligned - single source, not shifted
      // single source (shifted, top or bottom row)
      // double source (middle) both need shifting
      if (bYOff == 0) // simplest case
      {
        for (x = 0; x < bWidth; x++) {
          cOld = d[0];
          mask = pgm_read_byte(s);
          cNew = pgm_read_byte(s + 32);
          s++;
          cOld &= mask;
          cOld |= cNew;
          *d++ = cOld;
        }
      } else if (pObject->y + 8 < y) // only bottom half of sprite drawn
      {
        for (x = 0; x < bWidth; x++) {
          mask = pgm_read_byte(s);
          cNew = pgm_read_byte(s + 32);
          s++;
          mask >>= (8 - bYOff);
          mask |= (0xff << bYOff);
          cNew >>= (8 - bYOff);
          cOld = d[0];
          cOld &= mask;
          cOld |= cNew;
          *d++ = cOld;
        }                        // for x
      } else if (pObject->y > y) // only top half of sprite drawn
      {
        for (x = 0; x < bWidth; x++) {
          mask = pgm_read_byte(s);
          cNew = pgm_read_byte(s + 32);
          s++;
          mask <<= bYOff;
          mask |= (0xff >> (8 - bYOff)); // exposed bits set to 1
          cNew <<= bYOff;
          cOld = d[0];
          cOld &= mask;
          cOld |= cNew;
          *d++ = cOld;
        }    // for x
      } else // most difficult - part of top and bottom drawn together
      {
        byte mask2, cNew2;
        for (x = 0; x < bWidth; x++) {
          mask = pgm_read_byte(s);
          mask2 = pgm_read_byte(s + 16);
          cNew = pgm_read_byte(s + 32);
          cNew2 = pgm_read_byte(s + 48);
          s++;
          mask >>= (8 - bYOff);
          cNew >>= (8 - bYOff);
          mask2 <<= bYOff;
          cNew2 <<= bYOff;
          mask |= mask2; // combine top and bottom
          cNew |= cNew2;
          cOld = d[0];
          cOld &= mask;
          cOld |= cNew;
          *d++ = cOld;
        } // for x
      }
    } else // 8x8 sprite
    {
      s = (byte *)&ucSprites[bSprite * 16];
      bYOff = pObject->y & 7;
      bWidth = 8;
      if (128 - pObject->x < 8)
        bWidth = 128 - pObject->x;
      for (x = 0; x < bWidth; x++) {
        mask = pgm_read_byte(s);
        cNew = pgm_read_byte(s + 8);
        s++;
        if (bYOff) // needs to be shifted
        {
          if (pObject->y > y) {
            mask <<= bYOff;
            mask |= (0xff >> (8 - bYOff)); // exposed bits set to 1
            cNew <<= bYOff;
          } else {
            mask >>= (8 - bYOff);
            mask |= (0xff << bYOff);
            cNew >>= (8 - bYOff);
          }
        } // needs to be shifted
        cOld = d[0];
        cOld &= mask;
        cOld |= cNew;
        *d++ = cOld;
      } // for x
    }
  } // for each sprite
}

// Draw the playfield and sprites
void DrawPlayfield(byte bScrollX, byte bScrollY) {
  byte bTemp[SCREEN_WIDTH]; // holds data for the current scan line
  byte x, y, tx, ty;
  byte c, *s, *sNext, *d, bXOff, bYOff;
  int iOffset, iOffset2;

  bXOff = bScrollX & 7;
  bYOff = bScrollY & 7;

  ty = bScrollY >> 3;

  // draw the 8 lines of 128 (SCREEN_WIDTH) bytes
  for (y = 0; y < 8; y++) {
    memset(bTemp, 0, sizeof(bTemp));
    tx = bScrollX >> 3;

    if (ty >= 10) {
      ty -= 10; // wrap around
    }

    // Draw the playfield characters at the given scroll position
    d = bTemp;

    // partial characters vertically means a lot more work :(
    if (bYOff) {
      for (x = 0; x < 16; x++) {
        if (tx >= 18)
          tx -= 18; // wrap around
        iOffset = tx + ty * 18;
        iOffset2 = iOffset + 18; // next line
        if (iOffset2 >= 180)     // past bottom
          iOffset2 -= 180;
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[(c * 8) + bXOff];
        c = bPlayfield[iOffset2];
        sNext = (byte *)&ucTiles[(c * 8) + bXOff];
        DrawShiftedChar(s, sNext, d, bXOff, bYOff);
        d += (8 - bXOff);
        bXOff = 0;
        tx++;
      }

      // partial character left to draw
      if (d != &bTemp[SCREEN_WIDTH]) {
        bXOff = (byte)(&bTemp[SCREEN_WIDTH] - d);
        if (tx >= 18)
          tx -= 18;
        iOffset = tx + ty * 18;
        iOffset2 = iOffset + 18; // next line
        if (iOffset2 >= 180)     // past bottom
          iOffset2 -= 180;
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[c * 8];
        c = bPlayfield[iOffset2];
        sNext = (byte *)&ucTiles[c * 8];
        DrawShiftedChar(s, sNext, d, 8 - bXOff, bYOff);
      }
    // simpler case of vertical offset of 0 for each character
    } else {
      for (x = 0; x < 16; x++) {
        if (tx >= 18)
          tx -= 18; // wrap around
        iOffset = tx + ty * 18;
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[(c * 8) + bXOff];
        memcpy_P(d, s, 8 - bXOff);
        d += (8 - bXOff);
        bXOff = 0;
        tx++;
      }

      // partial character left to draw
      if (d != &bTemp[SCREEN_WIDTH]) {
        bXOff = (byte)(&bTemp[SCREEN_WIDTH] - d);
        if (tx >= 18)
          tx -= 18;
        iOffset = tx + ty * 18;
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[c * 8];
        memcpy_P(d, s, bXOff);
      }
    }

    DrawSprites(y * 8, bTemp, object_list, numberOfSprites);
    // Send it to the display
    oledSetPosition(0, y);
    I2CWriteData(bTemp, SCREEN_WIDTH);
    ty++;
  }
}


// Interrupt handler **********************************************
// Called when encoder value changes
// Button interrupt, INT0, PB2, pin7
void moveBackgroundTo(bool toRight) {
    //playerDirection = max(min((playerDirection + (Right ? 1 : -1)), 1000), 0);

    // TODO: Search something to refactor this mess:
    if (toRight) {
        iScrollX++;
    } else {
        iScrollX--;
    }
}

void moveBackground() {
    int a = PINB>>EncoderA & 1;
    int b = PINB>>EncoderB & 1;

    // A changed
    if (a != a0) {
        a0 = a;
        if (b != c0) {
            c0 = b;
            moveBackgroundTo(a == b);
        }
    }
}

void setup() {
   byte x, y, c, *d;
  // put your setup code here, to run once:
  delay(50); // wait for the OLED to fully power up
  oledInit(0, 0);
  oledFill(0);

  attachInterrupt(0, moveBackground, CHANGE); //INT0, PB2, pin7

 // Printing background
 for (y = 0; y < 10; y++) {
    d = &bPlayfield[y * 18];

    for (x = 0; x < 18; x++) {
      if (y < 3) {
        *d++ =  (x & 1) ? 15 : 60; // "\/" pattern for the first 3 rows (The first is hidden/offset top)
      } else if (y == 7) {
        *d++ = 3; // #
      } else {
        *d++ = 0; // Blank
      }
    }
  }

  memset(object_list, 0, sizeof(object_list));

  object_list[0].bType = 0x80; // big sprite
  object_list[0].x = 54;
  object_list[0].y = 40;

  iScrollX = iScrollY = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  int speed = 0;

  while (1) {
    DrawPlayfield(iScrollX, iScrollY);

    //if ((++speed & 3) == 0) { // Modulo 4 (25% speed)
    if ((++speed % 3) == 0) { // Modulo 3 (33% speed)
        iScrollX = (iScrollX >= 143) ? 0 : iScrollX; //144
        iScrollY = (iScrollY >= 79) ? 0 : iScrollY; //80

        if (analogRead(EncoderClick) < 940) {
            backgroundReset = 1;
        }
    }

    if (iScrollX < 0) {
        iScrollX = 142;
    }

    if (iScrollY < 0) {
        iScrollY = 78;
    }

    if (backgroundReset == 1) {
        backgroundReset = 0;
        backgroundPosX = 0;
        backgroundPosY = 0;
    }

#ifdef DEBUG
    initTXPin();
    Serial.print("iScrollX: ");
    writeUnsignedByte(iScrollX);
    write1Start8Data1StopNoParityWithCliSei('\n');
    Serial.print("iScrollY: ");
    writeUnsignedByte(iScrollY);
    Serial.print("\n\n");
#endif
  }
}