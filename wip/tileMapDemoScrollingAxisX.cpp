//#define DEBUG // To see serial output with 115200 baud at P2 -

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

// Screen resolution 128x64px
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define MODULE 8 // Module for tiles (bits) used for screen division / pages
#define EDGES 2 // Used only for pseudo buffer screen purposes

#define VIEWPORT_WIDTH SCREEN_WIDTH / MODULE
#define VIEWPORT_HEIGHT SCREEN_HEIGHT / MODULE

#define PLAYFIELD_ROWS VIEWPORT_HEIGHT // AXIS Y: horizontal rows, map height size; min 8+2 (SCREEN_HEIGHT / 8) + Edges
#define PLAYFIELD_COLS 25 // AXIS X: vertical cols, map width size; min 16+2 (SCREEN_WIDTH / 8) + Edges

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

static byte bPlayfield[PLAYFIELD_ROWS * PLAYFIELD_COLS];
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

// 16x16px Mario Bros
// 32 bytes of mask followed by 32 bytes of pattern
const byte ucBigSprites[] PROGMEM = {
  0xff, 0xff, 0xff, 0x0f, 0x07, 0x03, 0x03, 0x03,
  0x03, 0x03, 0x07, 0x07, 0xaf, 0xff, 0xff, 0xff,
  0xff, 0x73, 0x21, 0x00, 0x00, 0x00, 0x00, 0x80,
  0x00, 0x00, 0x00, 0x01, 0x23, 0x7f, 0xff, 0xff,

  0x00, 0x00, 0x00, 0x00, 0x60, 0xb0, 0xf8, 0x98,
  0xb8, 0xd0, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x8c, 0xd6, 0xf2, 0x3f, 0x1f,
  0x3c, 0xf2, 0xdc, 0x80, 0x00, 0x00, 0x00, 0x00,
};

const byte ucTiles[] PROGMEM = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Empty  (0)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Fill   (1)
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Fill   (2)
    0x7f, 0x21, 0x7d, 0x3d, 0x7d, 0x3f, 0x55, 0x00, // BRICK  (3)
    0x00, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x00, // =      (4)
    0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x01, 0x00, // /      (5)
    0x01, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, // \      (6)

    0x54, 0x00, 0x05, 0x00, 0x51, 0xa8, 0xf1, 0x18, // ? Box 1/4 (7)
    0x11, 0xa8, 0x51, 0xe0, 0x01, 0x04, 0x01, 0xfe, // ? Box 2/4 (8)
    0xd5, 0x80, 0xa0, 0x80, 0x80, 0x80, 0x80, 0x8a, // ? Box 3/4 (9)
    0xb5, 0xb7, 0x81, 0x81, 0x80, 0xa0, 0x80, 0xff, // ? Box 4/4 (10)

    0x6a, 0x81, 0x80, 0xb5, 0x8c, 0x81, 0xc0, 0xff, // Mini Question box (11)
    0xaa, 0xc1, 0xe8, 0xd5, 0xe8, 0xd5, 0xbe, 0x7f, // Mini Brick bezeled (12)

    0x2c, 0x5e, 0xa6, 0xe0, 0xc0, 0x0c, 0xcc, 0xee, // Mini Floating Wall Left Corner (13)
    0xae, 0x0e, 0xe0, 0xea, 0xee, 0x0c, 0xe0, 0xee, // Mini Floating Wall Middel (14)
    0xde, 0x18, 0xc2, 0x9e, 0xda, 0x74, 0x38, 0x00, // Mini Floating Wall Right Corner (15)

    0xaa, 0x5f, 0xaa, 0x5f, 0xaa, 0x5f, 0xaa, 0x5f, // Gradient 100-75% (16)
    0x8a, 0x00, 0x2a, 0x00, 0x8a, 0x00, 0x2a, 0x00, // Gradient 75-25% (17)


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
  // TODO: Esta es la secuencia de inicio de la OLED; documentar con lo que hace cada parametro.
  // TODO: Probablemente, manipular esta secuencia permita activar otras pantallas.
  unsigned char oled_initbuf[] = {
      0x00,       // Starting to send a commands sequence or script
      0xAE,       // Display off
      0xA8, 0x3F, // Set MUX Ratio 0x3F = 63; 0-63 (64) rows
      0xD3, 0x00, // Set display offset to 0
      0x40,       // Set Display Start Line to 0
      0xA1,       // Scan direction AXIS X: [0xA0, 0xA1]
      0xC8,       // Scan direction AXIS Y (invert screen): [0xC0, 0xC8]
      0xDA, 0x12, // Mapping COM pins
      0x81, 0xAA, // Setting constrast control
      0xA4,       // Disable Entire Display ON (GDDRAM)
      0xA6,       // Set normal display mode
      0xD5, 0x80, // Set OSC Frequency
      0x8D, 0x14, // Enable charge pump regulator
      0xAF,       // Set display on


      0x21, 0x00, 0x7f, 0x22, 0x00, 0x07,


      0x20, 0x00  // Set memory addressing mode to 'page addressing' [0x00 - Horizontal, 0x01 - Verticial]
  };

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
  oledWriteCommand(0x00 | (x & 0xf));        // lower col addr
  oledWriteCommand(0x10 | ((x >> 4) & 0xf)); // upper col addr
  iScreenOffset = (y * SCREEN_WIDTH) + x;
}

// Fill the frame buffer with a byte pattern
// e.g. all off (0x00) or all on (0xff)
void oledFill(unsigned char ucData) {
  int x, y;
  unsigned char temp[16]; // TODO: 16?? SCREEN_WIDTH / 2 (no edges) ??????

  memset(temp, ucData, 16);
  for (y = 0; y < 8; y++) {
    oledSetPosition(0, y); // set to (0,Y)
    for (x = 0; x < 8; x++) { // TODO: 8???? Debería ser 25, no????
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

  bXOff = bScrollX & 7; // MODULO 8
  bYOff = bScrollY & 7; // MODULO 8

  ty = bScrollY >> 3; // No aplica con scroll lateral, pero se necesita su valor más abajo. Podría ser 0.

  // draw the 8 rows
  // NOTE: No edges here!
  for (y = 0; y < VIEWPORT_HEIGHT; y++) { // SCREEN_HEIGHT / 8
    memset(bTemp, 0, sizeof(bTemp));
    tx = bScrollX >> 3; // Esta asignación debería estar fuera del bucle, no???

    if (ty >= PLAYFIELD_ROWS) {
      ty -= PLAYFIELD_ROWS;
    }

    // Draw the playfield characters at the given scroll position
    d = bTemp;

    // partial characters vertically means a lot more work :(
    // Con scroll horizontal, no se entra en esta función.
    if (1==2/*bYOff*/) {
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
        if (tx >= (VIEWPORT_WIDTH + EDGES))
          tx -= (VIEWPORT_WIDTH + EDGES); // wrap around
        iOffset = tx + ty * (VIEWPORT_WIDTH + EDGES);
        iOffset2 = iOffset + (VIEWPORT_WIDTH + EDGES); // next line
        if (iOffset2 >= 180)     // past bottom
          iOffset2 -= 180;
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[c * MODULE];
        c = bPlayfield[iOffset2];
        sNext = (byte *)&ucTiles[c * MODULE];
        DrawShiftedChar(s, sNext, d, MODULE - bXOff, bYOff);
      }
    // simpler case of vertical offset of 0 for each character
    } else {
      // Filling each col of the SCREEN_WIDTH
      for (x = 0; x < VIEWPORT_WIDTH; x++) {
        if (tx >= PLAYFIELD_COLS) {
          tx -= PLAYFIELD_COLS;
        }

        iOffset = tx + (ty * PLAYFIELD_COLS); // Next tile index to draw
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[(c * MODULE) + bXOff];
        memcpy_P(d, s, MODULE - bXOff);
        d += (MODULE - bXOff);
        bXOff = 0;
        tx++;
      }

      // partial character left to draw
      if (d != &bTemp[SCREEN_WIDTH]) {
        bXOff = (byte)(&bTemp[SCREEN_WIDTH] - d);
        if (tx >= PLAYFIELD_COLS) {
          tx -= PLAYFIELD_COLS;
        }
        iOffset = tx + ty * PLAYFIELD_COLS;
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[c * MODULE];
        memcpy_P(d, s, bXOff);
      }
    }

    DrawSprites(y * VIEWPORT_HEIGHT, bTemp, object_list, numberOfSprites);
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
   byte x, y, *d;

  delay(50); // wait for the OLED to fully power up
  oledInit(0, 0);
  oledFill(0xff);

  attachInterrupt(0, moveBackground, CHANGE); //INT0, PB2, pin7

  const byte tileMap[PLAYFIELD_ROWS][PLAYFIELD_COLS] PROGMEM = {
    {16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16},
    {17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17},
    {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {3,  0,  0,  0,  0,  0,  0, 11,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  4},
    {3,  0,  0,  0,  0,  0,  0, 11,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  4},
    {3,  0,  0,  0,  0,  0,  0, 11,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  4},
    {3,  0,  0,  0,  0,  0,  0, 11,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  4},
    {3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  0,  0,  0,  3,  3,  3}, // Floor brick
  };

  // Storing tyles
  // Realmente, esto es redundante: teníamos un tileMap (array 2D) que volcamos directamente
  // en este otro (array 1D) pero corrido...Se podría definir entonces directamente el tileMap en éste
  // y ahorramos complejidad...
  for (y = 0; y < PLAYFIELD_ROWS; y++) {
    d = &bPlayfield[y * PLAYFIELD_COLS];

    for (x = 0; x < PLAYFIELD_COLS; x++) {
      *d++ = tileMap[y][x];
    }
  }

  memset(object_list, 0, sizeof(object_list));

  object_list[0].bType = 0x80; // big sprite
  object_list[0].x = 14;
  object_list[0].y = 40;

  iScrollX = iScrollY = 0;
}

void loop() {
  // put your main code here, to run repeatedly:
  int speed = 0;

  while (1) {
    DrawPlayfield(iScrollX, iScrollY);

    // Desde aquí se puede definir la velocidad a la que responde el juego:
    // Estaría bien sacar el valor a una variable:
    // (++speed % 3) => Modulo 3 (33% speed)
    // (++speed & 3) => Modulo 4 (25% speed)
    if ((++speed % 3) == 0) { // Modulo 3 (33% speed)
      iScrollX = (iScrollX > PLAYFIELD_COLS * MODULE) ? 0 : iScrollX;
      iScrollY = (iScrollY > PLAYFIELD_ROWS * MODULE) ? 0 : iScrollY;

      if (analogRead(EncoderClick) < 940) {
        backgroundReset = 1;
      }
    }

    if (iScrollX < 0) {
      iScrollX = (PLAYFIELD_COLS * MODULE);
    }

    if (iScrollY < 0) {
      iScrollY = PLAYFIELD_ROWS * MODULE;
    }

    if (backgroundReset == 1) {
        backgroundReset = 0;
        backgroundPosX = 0;
        backgroundPosY = 0;
    }
  }
}