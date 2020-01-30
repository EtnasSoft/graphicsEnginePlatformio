/*#include <stdio.h>
int main () {
    printf("Hello World!");
    return 0;
}*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


// DEFS
// ------------------------------------------------------------------
typedef uint8_t byte;


// CONSTS
// ------------------------------------------------------------------
#define DEBUG
#define INCREASE 43
#define DECREASE 45


// Screen resolution 128x64px
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define MODULE 8 // Module for tiles (bits) used for screen division / pages
#define EDGES 2 // Used only for pseudo buffer screen purposes

#define VIEWPORT_WIDTH SCREEN_WIDTH / MODULE
#define VIEWPORT_HEIGHT SCREEN_HEIGHT / MODULE

// MAXIMUM: 16rows x 18cols => 288 byte + 128 byte (SSD1306 page)
#define PLAYFIELD_ROWS 8 // AXIS Y
#define PLAYFIELD_COLS VIEWPORT_WIDTH // AXIS X


// VARS
// ------------------------------------------------------------------
const byte ucTiles[] = {
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

// TIENE QUE TENER EL MISMO NUM. DE FILAS EXACTAS QUE INDICA EL ARRAY, SI PONE 10, 10 FILAS!
const byte tileMap[20][PLAYFIELD_COLS] = {
  {12, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12},
  {12,  0, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12},
  {12,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12},
  {12,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12},
  {12,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12},
  {12,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0,  0, 12},
  {12,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0,  0,  0, 12},
  {12, 12, 12, 12, 12, 12,  0,  0, 12,  0, 12, 12, 12, 12, 12, 12},  // -- FLOOR

  {12,  0,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0,  0, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0,  0, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0,  0, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12,  0,  0, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12,  0, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12, 12, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12, 12, 12, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12, 12, 12, 12, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0,  0, 12, 12, 12, 12, 12, 12},
  {12,  0,  0,  0,  0,  0,  0,  0,  0, 12, 12, 12, 12, 12, 12, 12},
  {12,  0,  0,  0,  0,  0,  0,  0, 12, 12, 12, 12, 12, 12, 12, 12},
};

static byte bPlayfield[PLAYFIELD_ROWS * PLAYFIELD_COLS];
static int iScrollX, iScrollY;
const char *BLANK = "·";
const char *FILL = "█";

// PROTOTYPES
// ------------------------------------------------------------------
void cls();
void DrawPlayfield(byte bScrollX, byte bScrollY);
void DrawShiftedChar(byte *s1, byte *s2, byte *d, byte bXOff, byte bYOff);
void PrintScreen();
void PrintScreenRowDetailed(byte data[SCREEN_HEIGHT * SCREEN_WIDTH]);

// MAIN
// ------------------------------------------------------------------
int main(void) {
  cls();

  // Demo purposes
  int c,
    flag = 0;

  byte x, y, *d;


  // Storing tyles
  for (y = 0; y < PLAYFIELD_ROWS; y++) {
    d = &bPlayfield[y * PLAYFIELD_COLS];

    for (x = 0; x < PLAYFIELD_COLS; x++) {
      memcpy(d++, &tileMap[y][x], 1);
    }
  }

  iScrollX = 0;
  iScrollY = 0;

  DrawPlayfield(iScrollX, iScrollY);

 /*while ((c = getchar()) != EOF) {
    switch (c) {
      case INCREASE:
        flag = 1;
        iScrollY++;
        break;
      case DECREASE:
        flag = 1;
        iScrollY--;
        break;
      default:
        flag = 0;
        break;
    }

    if (flag) {
      cls();
      DrawPlayfield(iScrollX, iScrollY);
    }
  }*/
  return 0;
}

void DrawShiftedChar(byte *s1, byte *s2, byte *d, byte bXOff, byte bYOff) {
  byte c, c2, z;
// s1 -> ucTiles
// s2 -> ucTiles
  for (z = 0; z < (8 - bXOff); z++) {
    c = *s1++;//pgm_read_byte(s1++);
    c >>= bYOff; // shift over
    c2 = *s2++; //pgm_read_byte(s2++);
    c2 <<= (8 - bYOff);
    *d++ = (c | c2);
  }
}

void DrawPlayfield(byte bScrollX, byte bScrollY) {
  byte bTemp[SCREEN_WIDTH]; // holds data for the current scan line
  byte x, y, tx;
  int ty, ty1;
  byte c, *s, *sNext, *d, bXOff, bYOff, /* MIOS -> */ sIndex;
  int iOffset, iOffset2;


  // Solo es cero cuando el scroll completa un MODULO su eje X (8 unidades)
  bXOff = bScrollX & (MODULE - 1);
  bYOff = bScrollY & (MODULE - 1);

  // ty: current row
  // Incrementa una unidad cada vez que el scroll completa un MODULO en su eje
  ty = bScrollY >> 3;
  //ty1 = bScrollY >> 3;

#ifdef DEBUG
  printf("\n\n\n");
  printf("PLAYFIELD_ROWS:\t\t%3i\t\tPLAYFIELD_COLS:\t\t%3i\n", PLAYFIELD_ROWS, PLAYFIELD_COLS);
  // printf("PLAYFIELD_COLS:\t\t%3i\n", PLAYFIELD_COLS);

  printf("SCREEN_HEIGHT:\t\t%3i\t\tVIEWPORT_HEIGHT:\t%3i\n", SCREEN_HEIGHT, VIEWPORT_HEIGHT);
  printf("SCREEN_WIDTH:\t\t%3i\t\tVIEWPORT_WIDTH:\t\t%3i\n", SCREEN_WIDTH, VIEWPORT_WIDTH);

  // printf("VIEWPORT_HEIGHT:\t%3i\n", VIEWPORT_HEIGHT);
  // printf("VIEWPORT_WIDTH:\t\t%3i\n", VIEWPORT_WIDTH);

  printf("iScrollX:\t%i\t\t\t\tiScrollY:\t%i\n", iScrollX, iScrollY);
  // printf("iScrollY:\t%i\n", iScrollY);

  printf("bYOff:\t\t%i\t\t\t\tbXOff:\t\t%i\n", bYOff, bXOff);
  // printf("bXOff:\t\t%i\n", bXOff);
  printf("ty:\t\t\t%i\n", ty);
  printf("\n---------------------------------------------------");
#endif

  // draw the 8 rows
  for (y = 0; y < VIEWPORT_HEIGHT; y++) {
    memset(bTemp, 0, sizeof(bTemp));

    // TODO: Esta comprobacion parece que tendría que ir fuera, pero se rompe la pantalla si se saca.
    if (ty >= PLAYFIELD_ROWS) {
      ty -= PLAYFIELD_ROWS;
    }

    /*if (ty1 >= 20) { // tenemos definido el tilemap con 20 filas...
      ty1 -= 20;
    }*/

    // TODO: Esta asignación debería estar fuera del bucle, no???
    tx = bScrollX >> 3; // Incrementa una unidad cada vez que el scroll completa un MODULO en X

    // Draw the playfield characters at the given scroll position
    d = bTemp;

    // partial characters vertically means a lot more work :(
    // Only for vertical scroll:
    printf("\n[%d]\t", ty);

    if (bYOff) {
      for (x = 0; x < VIEWPORT_WIDTH; x++) {
        if (tx >= PLAYFIELD_COLS) {
          tx -= PLAYFIELD_COLS; // wrap around
        }
        iOffset = tx + ty * PLAYFIELD_COLS;
        iOffset2 = iOffset + PLAYFIELD_COLS; // next line
        if (iOffset2 >= (PLAYFIELD_ROWS * PLAYFIELD_COLS))     // past bottom
          iOffset2 -= (PLAYFIELD_ROWS * PLAYFIELD_COLS);
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[(c * MODULE) + bXOff];
        c = bPlayfield[iOffset2];
        sNext = (byte *)&ucTiles[(c * MODULE) + bXOff];
        DrawShiftedChar(s, sNext, d, bXOff, bYOff);
        d += (MODULE - bXOff);
        bXOff = 0;

        // Tiles stored by index
        printf("%02i ", c);

        tx++;
      }

      // partial character left to draw
      if (d != &bTemp[SCREEN_WIDTH]) {
        bXOff = (byte)(&bTemp[SCREEN_WIDTH] - d);
        if (tx >= PLAYFIELD_COLS)
          tx -= PLAYFIELD_COLS;
        iOffset = tx + ty * PLAYFIELD_COLS;
        iOffset2 = iOffset + PLAYFIELD_COLS; // next line
        if (iOffset2 >= (PLAYFIELD_ROWS * PLAYFIELD_COLS))     // past bottom
          iOffset2 -= (PLAYFIELD_ROWS * PLAYFIELD_COLS);
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[c * MODULE];
        c = bPlayfield[iOffset2];
        sNext = (byte *)&ucTiles[c * MODULE];
        DrawShiftedChar(s, sNext, d, MODULE - bXOff, bYOff);
      }
    // simpler case of vertical offset of 0 for each character
    } else {
      //--------------------------------
      /*byte *d1;
      byte tempTy = ty1;
      byte nextRowTilemap;

      nextRowTilemap = (ty1 + 8);
      if (ty1 > 20) {
        nextRowTilemap = ty1 - 20;
      } else if (ty1 > 10) { // limite a ojo
        nextRowTilemap = ty1 - 10;
      }

      if (tempTy == 10) {
        tempTy = 0;
      } else if (tempTy == 0) {
        tempTy = 9;
      } else if (tempTy > 0) {
        tempTy -= 1;
      }

      byte yPos = tempTy * PLAYFIELD_COLS;

      d1 = &bPlayfield[yPos];

      for (byte x1 = 0; x1 < PLAYFIELD_COLS; x1++) {
        memcpy(d1++, &tileMap[nextRowTilemap][x1], 1);
      }*/
      //----------------------------------


      // Filling each col of the SCREEN_WIDTH
      for (x = 0; x < VIEWPORT_WIDTH; x++) {
        if (tx >= PLAYFIELD_COLS) {
          tx -= PLAYFIELD_COLS;
        }

        iOffset = tx + (ty * PLAYFIELD_COLS); // Next tile index to draw
        c = bPlayfield[iOffset];
        sIndex = (c * MODULE) + bXOff; // Useless var only for debug purposes :)
        s = (byte *)&ucTiles[(c * MODULE) + bXOff]; // Primer bit de tileMap necesario
        memcpy(d, s, MODULE - bXOff); // El tercer parametro indica cuantos bits copiamos desde "s"
        d += (MODULE - bXOff);
        bXOff = 0;
        tx++;


        // Tiles stored by index
        printf("%02i ", c);


        // All data in tiles!
        /*
        printf("[%02i]: ", c);
        for (byte z = 0; z < (MODULE - bXOff); z++) {
          printf("%03i, ", ucTiles[sIndex + z]);
        }
        printf("\n");
        */
      }

      // partial character left to draw
      // De momento el codigo no pasa por aqui.
      /*if (d != &bTemp[SCREEN_WIDTH]) {
        printf("\n\nPARTIAL CHARACTER LEFT \n\n");
        bXOff = (byte)(&bTemp[SCREEN_WIDTH] - d);

        if (tx >= PLAYFIELD_COLS) {
          tx -= PLAYFIELD_COLS;
        }

        iOffset = tx + ty * PLAYFIELD_COLS;
        c = bPlayfield[iOffset];
        s = (byte *)&ucTiles[c * MODULE];
        memcpy(d, s, bXOff);
      }*/
    }

    //printf("\n");

    // Esta funcion dibuja los sprites que de momento no necesitamos...
    //DrawSprites(y * VIEWPORT_HEIGHT, bTemp, object_list, numberOfSprites);

    // Send it to the display
    //oledSetPosition(0, y);
    //I2CWriteData(bTemp, SCREEN_WIDTH);
    ty++;
    //PrintScreen(bTemp);
  }
  printf("\n---------------------------------------------------\n");

  PrintScreen();
}


// DEMO ROUTINE
// ------------------------------------------------------------------
void PrintScreenRowDetailed(byte data[SCREEN_HEIGHT * SCREEN_WIDTH]) {
    printf("\n\n\nSCREEN REPRESENTATION:\n-----------------------------------\n");
    for (int x = 0; x < SCREEN_HEIGHT * SCREEN_WIDTH; x++) {
      printf("%03i ", data[x]);

      // Col separation (each byte)
      if (x > 0 && ((x+1) % MODULE == 0)) {
        printf("\n");
      }

      // Row separation (each row)
      if (x > 0 && ((x + 1) % (PLAYFIELD_COLS * MODULE) == 0)) {
        printf("\n");
      }
    }

    //printf("(%i): %i \n", y + 1, data[0]);
}

void PrintScreen() {
  int i;

  printf("\t╔════════════════╗");

  for(i = 0; i < PLAYFIELD_ROWS * PLAYFIELD_COLS; i++) {
    if (!(i & PLAYFIELD_COLS - 1)) {
      printf("%s\n%i\t║", ((i != 0) ? "║" : ""), (i / (PLAYFIELD_COLS - 1)) + 1);
    }

    printf("%s", bPlayfield[i] == 12 ? FILL : BLANK);
  }
  printf("║\n\t╚════════════════╝");
  printf("\nTOTAL : %i", i);
}

void cls() {
    printf("\e[1;1H\e[2J");
}