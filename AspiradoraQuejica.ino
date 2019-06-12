/* Robot aspiradora charlatana. Utiliza un Arduino UNO, y una shield para reproducir el audio desde una SD.
   Autor: Javier Vargas. El Hormiguero.
   https://creativecommons.org/licenses/by/4.0/
*/
//PINES
//Sensor
#define PinSensorIZQ A0
#define PinSensorDER A1
#define PinMotor A2
#define PinLed A3
//Spi
#define CLK 13 //SPI Clock, shared with SD card
#define MISO 12 //Input data, from VS1053/SD card
#define MOSI 11 //Output data, to VS1053/SD card
//Music shield
#define SHIELD_RESET -1 //VS1053 reset pin (unused!)
#define SHIELD_CS 7 //VS1053 chip select pin (output)
#define SHIELD_DCS 6 //VS1053 Data/command select pin (output)
#define CARDCS 4 //Card chip select pin
#define DREQ 3 //VS1053 Data request, ideally an Interrupt pin

//CONFIGURACION
//Sensor
#define UmbralSensor 500 //Umbral de tensión analógica de entrada del sensor de impacto
#define Antirrebotes 700 //(ms)
#define UmbralMotor 150 //Umbral de tensión analógica de entrada para el voltaje del motor 
#define MargenUmbral 50 //Margen entorno al umbral motor para evitar rebotes
//Comportamiento
#define tiempoMin 3000 //Tiempo de espera aleatoria minimo
#define tiempoMax 7000 //Tiempo de espera aleatoria maximo
#define PeriodoReducirEnfado 700 //(ms) Tiempo para reducir el enfado en 1
#define TiempoEnfadoChoque 5 //(s) Tiempo máximo entre choques para que se enfade
#define PuntosEnfadoChoque 10 //Puntos de enfado que se añaden como máximo entre choques
#define EnfadoN1 30 //Puntos de enfado máximo para nivel 1
#define EnfadoN2 60 //Puntos de enfado máximo para nivel 2 (El resto será nivel 3)
//Carpetas y audio
#define CarpetasMax 15 //Numero máximo de carpetas
#define AudiosMax 15 //Numero máximo de audios por carpeta
#define Volumen 1 //Volumen de salida (menos es más)
//Nombres de carpetas de la SD, guardades en memoria flash para liberar memoria dinámica
#include <avr/pgmspace.h>
const char string_0[] PROGMEM = "c1";
const char string_1[] PROGMEM = "c2";
const char string_2[] PROGMEM = "c3";
const char string_3[] PROGMEM = "o1";
const char string_4[] PROGMEM = "o2";
const char string_5[] PROGMEM = "o3";
const char string_6[] PROGMEM = "f1";
const char string_7[] PROGMEM = "f2";
const char string_8[] PROGMEM = "f3";
const char string_9[] PROGMEM = "ao1";
const char string_10[] PROGMEM = "ao2";
const char string_11[] PROGMEM = "ao3";
const char string_12[] PROGMEM = "af1";
const char string_13[] PROGMEM = "af2";
const char string_14[] PROGMEM = "af3";
const char *const Carpetas[] PROGMEM = {string_0, string_1, string_2, string_3, string_4, string_5, string_6, string_7, string_8, string_9, string_10, string_11, string_12, string_13, string_14};
char bufferCarpetas[5];
//Asociamos cada carpeta con una posicion del vector, debe coincidir con la posición anterior
#define choque1 0
#define choque2 1
#define choque3 2
#define encendido1 3
#define encendido2 4
#define encendido3 5
#define apagado1 6
#define apagado2 7
#define apagado3 8
#define aleatorioON1 9
#define aleatorioON2 10
#define aleatorioON3 11
#define aleatorioOFF1 12
#define aleatorioOFF2 13
#define aleatorioOFF3 14

//LIBRERIAS
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <Wire.h>

Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

byte numeroAudios[CarpetasMax] = {0}; //Numero de audios por carpeta
boolean audiosRep[CarpetasMax][AudiosMax] = {0}; //Audios reproducidos de cada carpeta

int nivelEnfado = 0; //Nivel de enfado de 0 a 100
boolean encendido = 0; //Robot limpiando

/////////////////////////////////////////////////////////////////
///////////////////////////// SETUP /////////////////////////////
/////////////////////////////////////////////////////////////////

void setup() {
  //Serial.begin(115200);
  
  //Led
  pinMode(PinLed, OUTPUT);

  //Inicio de reproductor de musica
  musicPlayer.begin();
  musicPlayer.setVolume(Volumen, Volumen);
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);

  //Inicio de la tarjeta SD
  if (!SD.begin(CARDCS)) SoundError();
  delay(10);

  //Numero de audios
  ContarAudios();

  //Semilla aleatoria
  randomSeed(analogRead(A4));

  delay(1000);
  musicPlayer.playFullFile("ok.mp3");

  //Serial.println(F("Encendido"));
}

/////////////////////////////////////////////////////////////////
////////////////////////////// LOOP /////////////////////////////
/////////////////////////////////////////////////////////////////

void loop() {
  //Reduce el nivel de enfado cada cierto tiempo
  ReducirEnfado();
  
  //Enciende el led durante el audio
  Led();
  
  //--------- COMENTARIOS AL GOLPEAR
  if (Impacto()) {
    if (encendido) {
      //Nivel de enfado aumentado
      AumentarEnfado();
      //Audios
      if (nivelEnfado < EnfadoN1) ReproducirAleatorio(choque1, 0);
      else if (nivelEnfado < EnfadoN2) ReproducirAleatorio(choque2, 0);
      else ReproducirAleatorio(choque3, 0);
      //Reinicio de la espera aleatoria
      EsperaAleatoria(1);
    }
  }

  //--------- COMENTARIOS ALEATORIOS
  if (EsperaAleatoria(0)) {
    //Encendido
    if (encendido) {
      //Audios
      if (nivelEnfado < EnfadoN1) ReproducirAleatorio(aleatorioON1, 0);
      else if (nivelEnfado < EnfadoN2) ReproducirAleatorio(aleatorioON2, 0);
      else  ReproducirAleatorio(aleatorioON3, 0);
    }
    //Apagado
    else {
      //Audios
      if (nivelEnfado < EnfadoN1)ReproducirAleatorio(aleatorioOFF1, 0);
      else if (nivelEnfado < EnfadoN2) ReproducirAleatorio(aleatorioOFF2, 0);
      else  ReproducirAleatorio(aleatorioOFF3, 0);
    }
  }

  //--------- COMENTARIOS DE ENCENDIDO
  if (BotonEncendido()) {
    if (nivelEnfado < EnfadoN1)ReproducirAleatorio(encendido1, 1);
    else if (nivelEnfado < EnfadoN2) ReproducirAleatorio(encendido2, 1);
    else  ReproducirAleatorio(encendido3, 1);
    //Reinicio de la espera aleatoria
    EsperaAleatoria(1);
  }

  //--------- COMENTARIOS DE APAGADO
  if (BotonApagado()) {
    if (nivelEnfado < EnfadoN1)ReproducirAleatorio(apagado1, 1);
    else if (nivelEnfado < EnfadoN2) ReproducirAleatorio(apagado2, 1);
    else  ReproducirAleatorio(apagado3, 1);
    //Reinicio de la espera aleatoria
    EsperaAleatoria(1);
  }
}

/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

/////////////// LED ///////////////////

void Led() {
  if (musicPlayer.playingMusic) digitalWrite(PinLed, HIGH);
  else digitalWrite(PinLed, LOW);
}

/////////////// SENSOR ///////////////////

boolean Impacto() {
  static unsigned long m = 0;
  //Impacto
  if (millis() > m) {
    if (analogRead(PinSensorIZQ) < UmbralSensor || analogRead(PinSensorDER) < UmbralSensor) {
      m = millis() + Antirrebotes;
      return 1;
    }
  }
  return 0;
}

boolean BotonEncendido() {
  if (!encendido) {
    if (analogRead(PinMotor) > UmbralMotor + MargenUmbral) {
      encendido = 1;
      return 1;
    }
  }
  return 0;
}

boolean BotonApagado() {
  if (encendido) {
    if (analogRead(PinMotor) < UmbralMotor - MargenUmbral) {
      encendido = 0;
      return 1;
    }
  }
  return 0;
}

/////////////// COMPORTAMIENTO ///////////////////

void AumentarEnfado() {
  static unsigned long m = 0;
  //Tiempo entre el ultimo golpe y el actual
  int t = constrain((millis() - m) / 1000, 0, TiempoEnfadoChoque);
  m = millis();
  //Nivel de enfado
  int enfado = map(t, 0, TiempoEnfadoChoque, PuntosEnfadoChoque, 0);
  nivelEnfado = nivelEnfado + enfado;
  nivelEnfado = constrain(nivelEnfado, 0, 100);
}

void ReducirEnfado() {
  static unsigned long m = 0;
  if (millis() / PeriodoReducirEnfado != m) {
    m = millis() / PeriodoReducirEnfado;
    if (nivelEnfado > 1) nivelEnfado--;
  }
}

boolean EsperaAleatoria(boolean res) {
  static unsigned long m = millis() + random(tiempoMin, tiempoMax);
  //Reiniciar tiempo
  if (res) m = millis() + random(tiempoMin, tiempoMax);
  //Tiempo aleatorio de espera alcanzado
  else if (millis() > m) {
    m = millis() + random(tiempoMin, tiempoMax);
    return 1;
  }
  return 0;
}

/////////////// AUDIO Y TARJETA SD ///////////////////
bool ReproducirAleatorio(byte carpeta, bool stp) {
  strcpy_P(bufferCarpetas, (char *)pgm_read_word(&(Carpetas[carpeta])));
  String Carpeta = String(bufferCarpetas);
  // Serial.print(Carpeta);
  //Ya esta reproduciendo un audio
  if (musicPlayer.playingMusic) {
    //Serial.println(F(" Audio en reproduccion..."));
    if (!stp) return 0;
    //Si stp vale 1, paramos el audio
    musicPlayer.stopPlaying();
  }
  //Serial.print(F(" Reproduciendo audio "));
  int audio = ArchivoAleatorio(carpeta);
  if (audio == -1) {
    //Serial.println(F("ERROR, no hay audios"));
    return 0;
  }
  //Direccion del archivo a abrir
  String dir = Carpeta + String("/") + String(audio) + String(".mp3");
  const char *c = dir.c_str();
  //Serial.println(dir);
  //Reproduccion del audio
  musicPlayer.startPlayingFile(c);
  return 1;
}

int ArchivoAleatorio(byte carpeta) {
  //Contamos los archivos de la carpeta
  byte nmax = numeroAudios[carpeta];
  //  Serial.print(F("("));
  //  Serial.print(nmax);
  //  Serial.print(F(")" ));
  if (nmax == 0) return -1;
  //Si se han reproducido todos los audios, reseteamos.
  boolean res = 1;
  for (byte i = 0; i < nmax; i++) {
    if (!audiosRep[carpeta][i]) {
      res = 0;
      break;
    }
  }
  if (res) {
    for (byte i = 0; i < nmax; i++) audiosRep[carpeta][i] = 0;
  }

  //Buscamos un numero aleatorio que no haya salido
  byte n;
  while (1) {
    n = random(nmax);
    if (!audiosRep[carpeta][n]) break;
  }
  audiosRep[carpeta][n] = 1;
  return n;
}

byte ContarArchivos(byte carpeta) {
  byte count = 0;
  strcpy_P(bufferCarpetas, (char*)pgm_read_word(&(Carpetas[carpeta])));
  File dir = SD.open("/" + String(bufferCarpetas));
  while (true) {
    //Abre el siguiente archivo
    File entry = dir.openNextFile();
    //No hay mas archivos
    if (!entry) {
      entry.close();
      break;
    }
    //Cuenta de archivos
    count++;
    entry.close();
  }
  dir.close();
  return count;
}

void ContarAudios() {
  for (int i = 0; i < CarpetasMax; i++)  {
    numeroAudios[i] = ContarArchivos(i);
    //    Serial.print(i);
    //    Serial.print(": ");
    //    Serial.println(numeroAudios[i]);
  }
}

void SoundError() {
  while (1) {
    musicPlayer.sineTest(0x44, 100);
    delay(1000);
  }
}
