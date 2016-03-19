/**
 * hoerborn.pde (.ino)
 * Die Firmware für den Nachbau des Hoerbert Musikplayers.
 * Grundlage bildet das ASA1 MP3-Shield der Firma ELV.
 *
 * Die Dateien muessen in 9 Ordner mit Namen 1-9 abgelegt und aufsteigend sortiert sein.
 *
 * @mc       Arduino/RBBB (ATMEGA328)
 * @autor    Nicolas Born / nicolasborn@gmail.com
 * @version  0.1
 * @created  19.03.2016
 * @updated  19.03.2016
 *
 * Versionshistorie:
 * V 0.1:   - Erste Version
 **/

#include <SD.h>
#include <SPI.h>
#include <AudioShield.h>
#include <EEPROM.h>

// Variablen fuer Filehandling
char filename[13];
int currentFolder = 0;
int numberOfFiles[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

// Debug Modus (bezieht sich nur auf die Logausgabe
boolean debug = true;

// Variablen fuer Buttonhandling
int button = -1;
int previousButtonPressed = -1;
unsigned long lastButtonEvent;

// Variablen bezueglich pausieren von Songs
boolean paused = false;
long filePosition = 0;


void setup() {
  digitalWrite(6, HIGH);
  Serial.begin(9600);
  println("Los geht's");
  //SD-Karte initialisieren
  if ( SD.begin( SD_CS ) == false )
  {
    println("Karte fehlt oder ist fehlerhaft");
    return;
  }
  println("Karte initialisiert.");
  count();
  //MP3-Decoder initialisieren
  VS1011.begin();
  VS1011.SetVolume(10, 10);
  randomSeed(analogRead(0));
}

void loop() {
  if (checkAndSetButtonPressed() && button < 10 && numberOfFiles[button] > 0) {
    sprintf(filename, "%d/%02d.mp3", button, 1);
  }

  // Puffer für MP3-Decoder anlegen
  // MP3-Decoder erwartet Daten immer in 32 Byte Blöcken
  unsigned char buffer[32];

  //Datei öffnen und abspielen
  if (!SD.exists(filename) || paused) {
    return;
  }
  else if (File SoundFile = SD.open(filename)) {
    LED_BLUE_ON;

    if (filePosition > 0) {
      SoundFile.seek(filePosition);
      println("Spiele Datei " + String(filename) + " weiter ab Position " + String(filePosition));
      filePosition = 0;
    } else {
      println("Spiele Datei " + String(filename));
    }

    VS1011.UnsetMute();
    while (!paused && SoundFile.available()) {
      SoundFile.read(buffer, sizeof(buffer));
      VS1011.Send32(buffer);
      if (checkAndSetButtonPressed() && button < 10) {
        if (paused && filePosition == 0) {
          filePosition = SoundFile.position();
          println("Pausiert an Position " + String(filePosition));
        } else if (!paused){
          sprintf(filename, "%d/%02d.mp3", button, 1);
        }
        break;
      }
    }

    VS1011.Send2048Zeros();
    VS1011.SetMute();

    LED_BLUE_OFF;
  }

  // Wartezeit zwischen Durchläufen 500ms
  // delay( 500 );
}

// Prueft welcher Knopf gedrueckt wurde
// Gibt -1 zurueck falls kein Knopf gedrueckt wurde
int checkButtonPressed() {
  int value = 470;
  value = analogRead(A5);
  if (value > 870) return 10;
  if (value > 700) return 7;
  if (value > 670) return 4;
  if (value > 610) return 1;
  if (value > 550) return 2;
  if (value > 500) return 5;
  if (value > 460) return -1;
  if (value > 410) return 8;
  if (value > 360) return 11;
  if (value > 300) return 9;
  if (value > 200) return 6;
  if (value > 120) return 3;
  return -1;
}

// Gibt true zurueck falls ein neuer Knopf gedrueckt wurde
boolean checkAndSetButtonPressed() {
  int newButtonPressed = checkButtonPressed();
  if (newButtonPressed == -1){
    return false;
  }
  if (button == newButtonPressed) {
    if (paused && button <10 && (millis()-lastButtonEvent)>1000){
      lastButtonEvent = millis();
      paused = false;
      return true;
    }
    if (!paused && button<10 && (millis()-lastButtonEvent)>1000){
      
      lastButtonEvent = millis();
      paused = true;
      VS1011.Send2048Zeros();
      VS1011.SetMute();
      return true;
    }
    return false;
  }
  previousButtonPressed = newButtonPressed;
  button = newButtonPressed;
  lastButtonEvent = millis();
  paused = false;
  filePosition = 0;
  return true;
}

// Zaehlt die Anzahl der Dateien auf der SD Karte und legt die Anzahl im Array ab
// Annahme: die Dateien sind in 9 Ordner mit Namen 1-9 abgelegt und sind aufsteigend sortiert (01-99.mp3)
void count() {
  int folder = 1;
  int counter = 1;
  char name[13];
  while (folder < 10) {
    counter = 1;
    while (true) {
      sprintf(name, "%d/%02d.mp3", folder, counter++);
      if (!SD.exists(name) || counter > 99) {
        println("Fuer Ordner " + String(folder) + " wurden " + String(counter - 2) + " Dateien gefunden.");
        numberOfFiles[folder++] = counter - 2;
        break;
      }
    }
  }
}

void println(String logMsg) {
  if (!debug) return;
  Serial.println(logMsg);
}
