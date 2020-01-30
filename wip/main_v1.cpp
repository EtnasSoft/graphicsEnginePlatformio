#include <Arduino.h>
#include <avr/power.h>
#define I2C_SCREEN_ADDRESS 0x3C

// Demo data
int initX = 2;
int initY = 3;

int playerX;
int playerY;
int playerWidth = 16;
int vectorX = 1;

int alienX;
int alienY;
int vectorAlienX = 1;
int vectorAlienY = 1;

bool reverse;

const uint32_t items[2] = {0x00, 0xFF};
char screenBuffer [128];

// Convenience definitions for PORTB
// Note that we go direct to port rather than using digitalWrite as it's much faster
#define DIGITAL_WRITE_HIGH(PORT) PORTB |= (1 << PORT)
#define DIGITAL_WRITE_LOW(PORT) PORTB &= ~(1 << PORT)

#define SSD1306_SCL   PORTB2  // SCL
#define SSD1306_SDA   PORTB0  // SDA
#define SSD1306_SA    0x78  // Slave address (?)

// Prototypes for SSD1306
void ssd1306_init( void );
void ssd1306_xfer_start( void );
void ssd1306_xfer_stop( void );
void ssd1306_send_byte( uint8_t byte );
void ssd1306_send_array( const uint8_t arr [12], bool reverse = false );
void ssd1306_send_command( uint8_t command );
void ssd1306_send_data_start( void );
void ssd1306_send_data_stop( void );
void ssd1306_setpos( uint8_t x, uint8_t y );
void ssd1306_fillscreen( uint8_t fill_Data );

// SSD1306 Functions
void ssd1306_init( void ) {
    DDRB |= ( 1 << SSD1306_SDA ); // Set port as output
    DDRB |= ( 1 << SSD1306_SCL ); // Set port as output

    ssd1306_send_command( 0xAE ); // display off
    ssd1306_send_command( 0x00 ); // Set Memory Addressing Mode
    ssd1306_send_command( 0x10 ); // 00,Horizontal Addressing Mode;01,Vertical Addressing Mode;10,Page Addressing Mode (RESET);11,Invalid
    ssd1306_send_command( 0x40 ); // Set Page Start Address for Page Addressing Mode,0-7
    ssd1306_send_command( 0x81 ); // Set COM Output Scan Direction
    ssd1306_send_command( 0xCF ); // ---set low column address
    ssd1306_send_command( 0xA1 ); // ---set high column address
    ssd1306_send_command( 0xC8 ); // --set start line address
    ssd1306_send_command( 0xA6 ); // --set contrast control register
    ssd1306_send_command( 0xA8 );
    ssd1306_send_command( 0x3F ); // --set segment re-map 0 to 127
    ssd1306_send_command( 0xD3 ); // --set normal display
    ssd1306_send_command( 0x00 ); // --set multiplex ratio(1 to 64)
    ssd1306_send_command( 0xD5 ); //
    ssd1306_send_command( 0x80 ); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content
    ssd1306_send_command( 0xD9 ); // -set display offset
    ssd1306_send_command( 0xF1 ); // -not offset
    ssd1306_send_command( 0xDA ); // --set display clock divide ratio/oscillator frequency
    ssd1306_send_command( 0x12 ); // --set divide ratio
    ssd1306_send_command( 0xDB ); // --set pre-charge period
    ssd1306_send_command( 0x40 ); //
    ssd1306_send_command( 0x20 ); // --set com pins hardware configuration
    ssd1306_send_command( 0x02 );
    ssd1306_send_command( 0x8D ); // --set vcomh
    ssd1306_send_command( 0x14 ); // 0x20,0.77xVcc
    ssd1306_send_command( 0xA4 ); // --set DC-DC enable
    ssd1306_send_command( 0xA6 ); //
    ssd1306_send_command( 0xAF ); // --turn on oled panel
}

void ssd1306_xfer_start( void ) {
    DIGITAL_WRITE_HIGH( SSD1306_SCL ); // Set to HIGH
    DIGITAL_WRITE_HIGH( SSD1306_SDA ); // Set to HIGH
    DIGITAL_WRITE_LOW( SSD1306_SDA ); // Set to LOW
    DIGITAL_WRITE_LOW( SSD1306_SCL ); // Set to LOW
}

void ssd1306_xfer_stop( void ) {
    DIGITAL_WRITE_LOW( SSD1306_SCL ); // Set to LOW
    DIGITAL_WRITE_LOW( SSD1306_SDA ); // Set to LOW
    DIGITAL_WRITE_HIGH( SSD1306_SCL ); // Set to HIGH
    DIGITAL_WRITE_HIGH( SSD1306_SDA ); // Set to HIGH
}

void ssd1306_send_byte( uint8_t byte ) {
    uint8_t i;

    for ( i = 0; i < 8; i++ ) {
        if ( ( byte << i ) & 0x80 )
            DIGITAL_WRITE_HIGH( SSD1306_SDA );
        else
            DIGITAL_WRITE_LOW( SSD1306_SDA );

        DIGITAL_WRITE_HIGH( SSD1306_SCL );
        DIGITAL_WRITE_LOW( SSD1306_SCL );
    }

    DIGITAL_WRITE_HIGH( SSD1306_SDA );
    DIGITAL_WRITE_HIGH( SSD1306_SCL );
    DIGITAL_WRITE_LOW( SSD1306_SCL );
}

void ssd1306_send_array( const uint8_t arr [12], bool reverse = false ) {
    if (!reverse) {
      for ( int i = 0; i < 12; i++ ) {
        ssd1306_send_byte( arr[i] );
      }
    } else {
      for ( int i = 11; i >= 0; i-- ) {
        ssd1306_send_byte( arr[i] );
      }
    }
}

void ssd1306_send_command( uint8_t command ) {
    ssd1306_xfer_start();
    ssd1306_send_byte( SSD1306_SA ); // Slave address, SA0=0
    ssd1306_send_byte( 0x00 ); // write command
    ssd1306_send_byte( command );
    ssd1306_xfer_stop();
}

void ssd1306_send_data_start( void ) {
    ssd1306_xfer_start();
    ssd1306_send_byte( SSD1306_SA );
    ssd1306_send_byte( 0x40 ); //write data
}

void ssd1306_send_data_stop( void ) {
    ssd1306_xfer_stop();
}

void ssd1306_setpos( uint8_t x, uint8_t y ) {
    if ( y > 7 )
        return;

    ssd1306_xfer_start();
    ssd1306_send_byte( SSD1306_SA ); // Slave address, SA0=0
    ssd1306_send_byte( 0x00 );     // write command

    ssd1306_send_byte( 0xb0 + y );
    ssd1306_send_byte( ( ( x & 0xf0 ) >> 4 ) | 0x10 ); // |0x10
    ssd1306_send_byte( ( x & 0x0f ) | 0x01 );    // |0x01

    ssd1306_xfer_stop();
}

void ssd1306_fillscreen( uint8_t fill_Data ) {
    uint8_t m, n;

    for ( m = 0; m < 8; m++ ) {
        ssd1306_send_command( 0xb0 + m ); //page0-page1
        ssd1306_send_command( 0x00 ); //low column start address
        ssd1306_send_command( 0x10 ); //high column start address
        ssd1306_send_data_start();

        for ( n = 0; n < 128; n++ ) {
            ssd1306_send_byte( fill_Data );
        }

        ssd1306_send_data_stop();
    }
}

// Characters :)
// Los binarios se escriben de abajo a arriba:
/*
    ■ ■ ■ ■ ■ ■ ■ ■   => B00000001
    □ ■ ■ ■ ■ ■ ■ □   => B00000011
    □ □ ■ ■ ■ ■ □ □   => B00000111
    □ □ □ ■ ■ □ □ □   => B00011111
    □ □ □ ■ ■ □ □ □   => B00011111
    □ □ □ □ □ □ □ □   => B00000111
    □ □ □ □ □ □ □ □   => B00000011
    □ □ □ □ □ □ □ □   => B00000001
*/
void sendBlock( int fill ) {
    if ( fill == 1 ) { // Space invader - Squid
        ssd1306_send_byte( 0x98 ); //B10011000
        ssd1306_send_byte( 0x5C ); //B01011100
        ssd1306_send_byte( 0xB6 ); //B10110110
        ssd1306_send_byte( 0x5F ); //B01011111
        ssd1306_send_byte( 0x5F ); //B01011111
        ssd1306_send_byte( 0xB6 ); //B10110110
        ssd1306_send_byte( 0x5C ); //B01011100
        ssd1306_send_byte( 0x98 ); //B10011000
    } else if ( fill == 2 ) { // Medusa
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00110000 );
        ssd1306_send_byte( B00111110 );
        ssd1306_send_byte( B10110011 );
        ssd1306_send_byte( B01011101 );
        ssd1306_send_byte( B01011101 );
        ssd1306_send_byte( B10110011 );
        ssd1306_send_byte( B00111110 );
        ssd1306_send_byte( B00110000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
    } else if ( fill == 3 ) { // Ship
        ssd1306_send_byte( B00011000 );
        ssd1306_send_byte( B00111000 );
        ssd1306_send_byte( B00110100 );
        ssd1306_send_byte( B00110100 );
        ssd1306_send_byte( B00110100 );
        ssd1306_send_byte( B00110100 );
        ssd1306_send_byte( B00111000 );
        ssd1306_send_byte( B00011000 );
    } else if ( fill == 4 ) { // Frogger
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B11011000 );
        ssd1306_send_byte( B00111100 );
        ssd1306_send_byte( B11011010 );
        ssd1306_send_byte( B01111110 );
        ssd1306_send_byte( B11000010 );
        ssd1306_send_byte( B00111100 );
        ssd1306_send_byte( B11011010 );
    } else if ( fill == 5 ) { // Frogger block 2
        ssd1306_send_byte( B00011011 );
        ssd1306_send_byte( B00111101 );
        ssd1306_send_byte( B01011011 );
        ssd1306_send_byte( B01111110 );
        ssd1306_send_byte( B01000011 );
        ssd1306_send_byte( B00111101 );
        ssd1306_send_byte( B01011011 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
    } else if ( fill == 0 ) {
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
        ssd1306_send_byte( B00000000 );
    }
}

const uint8_t phantom[12] = {0x00, 0x00, 0x7C, 0xF6, 0x66, 0xFF, 0x7F, 0xF6, 0x66, 0xFC, 0x00, 0x00};
const uint8_t alien[12] = {0x00, 0x00, 0x98, 0x5C, 0xB6, 0x5F, 0x5F, 0xB6, 0x5C, 0x98, 0x00, 0x00};
const uint8_t player[12] = {0x00, 0x00, 0x7C, 0xF6, 0x66, 0xFF, 0x7F, 0xF6, 0x66, 0xFC, 0x00, 0x00};

void renderPlayer() {
    reverse = !reverse;
    ssd1306_setpos( playerX, playerY );
    ssd1306_send_data_start();
    ssd1306_send_array( player, reverse );
    ssd1306_send_data_stop();
}

void renderAlien() {
    ssd1306_setpos(0, 0);
    ssd1306_send_data_start();
    ssd1306_send_array(alien);
    ssd1306_send_data_stop();
}

void renderPhantom() {
  ssd1306_setpos(5,5);
  ssd1306_send_data_start();
  ssd1306_send_array(phantom);
  ssd1306_send_data_stop();
}

void setup() {
    if (F_CPU == 16000000) {
        clock_prescale_set(clock_div_1);
    }

    playerX = 96;
    playerY = 7;
    alienX = 0;
    alienY = 1;
    reverse = false;

    ssd1306_init();
    ssd1306_fillscreen( 0x00 );

    renderPlayer();
    // renderAlien();
    // renderPhantom();
}

void loop() {
    if (
        playerX > ( 126 - playerWidth ) ||
        playerX < 0
    ) {
        vectorX *= -1;
    }

    if (
        alienX > ( 126 - 10 ) ||
        alienX < 0
    ) {
        vectorAlienX *= -1;
    }

    if (
        alienY > ( 6 ) ||
        alienY < 1
    ) {
        vectorAlienY *= -1;
    }

    playerX += vectorX;


    ssd1306_setpos( alienX, alienY );
    ssd1306_send_data_start();
    sendBlock(0);
    ssd1306_send_data_stop();

    // alienX += vectorAlienX;
    alienY += vectorAlienY;

    // renderAlien();
    renderPlayer();
    // renderPhantom();

    ssd1306_setpos( alienX, alienY );
    ssd1306_send_data_start();
    sendBlock(2);
    ssd1306_send_data_stop();

    // The reverse/mirror effect on scroll is not visible with a lower delay
    delay( 50 );
}