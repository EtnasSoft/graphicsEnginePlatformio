#include <Arduino.h>
#include <Wire.h>
#include <avr/power.h>
#define I2C_SCREEN_ADDRESS 0x3C

// Demo data
int initX = 2;
int initY = 3;
int vectorX = 1;
int vectorY = 1;
const uint32_t items[2] = {0x00, 0xFF};
char screenBuffer [128];

void initScreen() {
  Wire.begin(); // Iniciar la comunicacion I2C de Arduino

  // Iniciar la comunicacion con la pantalla
  Wire.beginTransmission(I2C_SCREEN_ADDRESS);

  // Le decimos a la pantalla que viene una lista de comandos de configuracion
  Wire.write(0x00);

  // Apagar la pantalla
  Wire.write(0xAE);

  // Establecer el maximo de filas a 0x3F = 63
  // es decir, ira de 0 a 63, por tanto tenemos 64 filas de pixeles
  Wire.write(0xA8);
  Wire.write(0x3F);

  // Poner el offset a 0
  Wire.write(0xD3);
  Wire.write(0x00);

  // Poner el comienzo de linea a 0
  Wire.write(0x40);

  // Invertir el eje X de pantalla, por si esta girada.
  // Puedes cambiarlo por 0xA0 si necesitas cambiar la orientacion
  Wire.write(0xA1);

  // Invertir el eje Y de la patnalla
  // Puedes cambiarlo por 0xC0 si necesitas cambiar la orientacion
  Wire.write(0xC8);

  // Mapear los pines COM
  Wire.write(0xDA);
  Wire.write(0x12);
  // Al parecer, la unica configuracion que funciona con mi modelo es 0x12, a
  // pesar de que en la documentacion dice que hay que poner 0x02

  // Configurar el contraste
  Wire.write(0x81);
  Wire.write(0x00); // Este valor tiene que estar entre 0x00 (min) y 0xFF (max)

  // Este comando ordena al chip que active el output de la pantalla en funcion
  // del contenido almacenado en su GDDRAM
  Wire.write(0xA4);

  // Poner la pantalla en modo Normal
  Wire.write(0xA6);

  // Establecer la velocidad del Oscilador
  Wire.write(0xD5);
  Wire.write(0x80);

  // Activar el 'charge pump'
  Wire.write(0x8D);
  Wire.write(0x14);

  // Encender la pantalla
  Wire.write(0xAF);

  // Como extra, establecemos el rango de columnas y paginas
  Wire.write(0x21); // Columnas de 0 a 127
  Wire.write(0x00);
  Wire.write(0x7F);
  Wire.write(0x22); // Paginas de 0 a 7
  Wire.write(0x00);
  Wire.write(0x07);

  // Modo de escritura horizontal
  // en mi modelo no haria falta enviar este comando (por defecto utiliza este
  // modo)
  Wire.write(0x20);
  Wire.write(0x00); // 00 horizontal,  01 vertical

  // Cerrar la comunicacion
  Wire.endTransmission();
}

void clearScreen() {
  for (int i = 0; i < 1024; i++) {
    Wire.beginTransmission(I2C_SCREEN_ADDRESS);
    Wire.write(0x40);
    Wire.write(0x00);
    Wire.endTransmission();
  }
}

void printBuffer() {
  char c;

  for(int i = 0; i < 128; i++) {
    c = screenBuffer[i]; //Guardar el caracter que hay que escribir

    for (int j = 0; j < 8; j++) {
      Wire.beginTransmission(I2C_SCREEN_ADDRESS);
      Wire.write(0x40);
      Wire.write(items[c]);
      Wire.endTransmission();
    }
  }
}

void addItem(int item, int posX, int posY) {
  screenBuffer[(posX)+(posY*16)] = item;
}

void setup() {
  if (F_CPU == 16000000) {
    clock_prescale_set(clock_div_1);
  }

  initScreen();
  clearScreen();

  // Fill the buffer
  for(int i = 0; i < 128; i++) {
    screenBuffer[i] = 0; //blank
  }
}

void loop() {
  addItem(0, initX, initY);

  if (initX > 13 || initX < 1) {
    vectorX *= -1;
  }

  if (initY > 6 || initY < 1) {
    vectorY *= -1;
  }

  initX += vectorX;
  initY += vectorY;

  addItem(1, initX, initY);

  printBuffer();
}