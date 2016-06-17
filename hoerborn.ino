/**
 * hoerborn.pde (.ino)
 * Die Firmware für den Nachbau des Hoerbert Musikplayers.
 * Grundlage bildet das ASA1 MP3-Shield der Firma ELV.
 *
 * Die Dateien muessen in 9 Ordner mit Namen 1-9 abgelegt und aufsteigend sortiert sein.
 *
 * @mc       Arduino/RBBB (ATMEGA328)
 * @autor    Nicolas Born / nicolasborn@gmail.com
 * @version  1.0
 * @created  19.03.2016
 * @updated  17.06.2016
 *
 * Versionshistorie:
 * V 0.1:   - Erste Version
 * V 1.0:   - Erster Release
 **/

#include <SD.h>
#include <SPI.h>
#include <AudioShield.h>
#include <EEPROM.h>

// Konstanten
const int buttonsPin = A5;
const int volumePin = A4;

// Variablen fuer Filehandling
char filename[15];
int currentFolder = -1;
int currentFile = 1;
int numberOfFiles[] = {0, 0, 0, 0, 0, 0, 0, 0, 0};


// Debug Modus (bezieht sich nur auf die Logausgabe
boolean debug = true;

// Variablen fuer Buttonhandling
int button = -1;

// Variablen bezueglich pausieren von Songs
boolean paused = false;
long filePosition = 0;

// Timer
unsigned long lastButtonEvent;
unsigned long lastVolumeEvent;
unsigned long lastEepromEvent;


// Buffer fuer Wiedergabe
unsigned char buffer[32];


void setup() {
  digitalWrite(6, HIGH);
  Serial.begin(9600);
  
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  //beide LEDs ausschalten
  LED_BLUE_OFF;
  LED_RED_OFF;
  
  pinMode(buttonsPin, INPUT);
  pinMode(volumePin, INPUT);
  
  println("Start Hoerborn");
  //SD-Karte initialisieren
  if ( SD.begin( SD_CS ) == false )
  {
    println("Karte fehlt oder ist fehlerhaft");
    return;
  }
  println("Karte initialisiert.");
  
  countNumberOfFiles();
  
  //MP3-Decoder initialisieren
  VS1011.begin();
  
  loadPreviouslyPlayedSong();
}

void loop() {
  checkAndSetButtonPressed();
  if (currentFolder==-1) return;
  if (paused) return;
  
  chooseFile();
  checkAndSetVolume();

  //Datei öffnen und abspielen
  if (!SD.exists(filename)) {
    LED_RED_ON;
    delay(500);
    LED_RED_OFF;
    return;
  }
  else if (File SoundFile = SD.open(filename)) {
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
      checkAndSetVolume();
      if (checkAndSetButtonPressed()) {
        if (paused && filePosition == 0) {
          filePosition = SoundFile.position();
          saveSongAndPositionInEeprom(filePosition);
          println("Pausiert an Position " + String(filePosition));
        }
        VS1011.Send2048Zeros();
        VS1011.SetMute();
        SoundFile.close();
        return;
      }
      if (millis()-lastEepromEvent>60000){
        saveSongAndPositionInEeprom(SoundFile.position());
      }
    }

    VS1011.Send2048Zeros();
    VS1011.SetMute();
    SoundFile.close();

    chooseNextFile();
  }
}

// Gibt true zurueck falls ein neuer Knopf gedrueckt wurde
boolean checkAndSetButtonPressed() {
  int newButtonPressed = checkButtonPressed();
  if (newButtonPressed == -1){
    return false;
  }
  if (currentFolder == newButtonPressed) {
    return pauseUnpause();
  }
  paused = false;
  if (newButtonPressed > 9){
    return nextPreviousSong(newButtonPressed);
  }
  button = newButtonPressed;
  currentFolder = newButtonPressed;
  currentFile = 1;
  filePosition = 0;
  lastButtonEvent = millis();
  saveSongAndPositionInEeprom(0);
  return true;
}

void checkAndSetVolume() {
  if (millis()-lastVolumeEvent<100){
    return;
  }
  int volume = analogRead(volumePin);
  volume = map(volume, 1023, 0, 100, 0);
  VS1011.SetVolume(volume, volume);
  lastVolumeEvent = millis();
}

// Pausiert den Song oder beendet die Pause,
// wenn genuegend Zeit zwischen dem letzten Button vergangen ist (mehr als 1 Sekunde)
// gibt true zurueck falls sich am Pause-Status etwas geaendert hat
boolean pauseUnpause(){
  if (millis()-lastButtonEvent<1000){
    return false; 
  }
  lastButtonEvent = millis();
  paused = !paused;
  return true;
}

// Waehlt das naechste/letzte Lied oder springt zum naechsten Ordner falls kein Lied mehr im Ordner vorhanden ist
boolean nextPreviousSong(int newButtonPressed){
  if (millis()-lastButtonEvent<1000){
    return false;
  }
  if (newButtonPressed == 11){
    chooseNextFile();
  }
  filePosition = 0;
  lastButtonEvent = millis();
  return true;
}

void chooseNextFile(){
  currentFile++;
  // Pruefe ob Datei existiert, ansonsten wird die naechste gespeichert.
  while (true){
    if (numberOfFiles[currentFolder-1]>0 && numberOfFiles[currentFolder-1]>=currentFile){
      break;
    }
    currentFolder = currentFolder==9?1:currentFolder+1;
    currentFile = 1;
    button = currentFolder;
  }
  saveSongAndPositionInEeprom(0);
}

// Prueft welcher Knopf gedrueckt wurde
// Gibt -1 zurueck falls kein Knopf gedrueckt wurde
int checkButtonPressed() {
  int value = 470;
  value = analogRead(buttonsPin);
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

void chooseFile(){
  sprintf(filename, "%d/%02d.mp3", currentFolder, currentFile);
}  

// Zaehlt die Anzahl der Dateien auf der SD Karte und legt die Anzahl im Array ab
// Annahme: die Dateien sind in 9 Ordner mit Namen 1-9 abgelegt und sind aufsteigend sortiert (01-99.mp3)
void countNumberOfFiles() {
  int folder = 1;
  int counter = 1;
  char name[13];
  while (folder < 10) {
    counter = 1;
    while (true) {
      sprintf(name, "%d/%02d.mp3", folder, counter++);
      if (!SD.exists(name) || counter > 99) {
        println("Fuer Ordner " + String(folder) + " wurden " + String(counter - 2) + " Dateien gefunden.");
        numberOfFiles[folder++-1] = counter - 2;
        break;
      }
    }
  }
}

void loadPreviouslyPlayedSong(){
  int folderFromEeprom = EEPROM.read(0);
  int fileFromEeprom = EEPROM.read(1);
  
  if (folderFromEeprom<=0 || fileFromEeprom<=0 || folderFromEeprom>10 || fileFromEeprom>99){
    return;
  }
  currentFolder = folderFromEeprom;
  currentFile = fileFromEeprom;
  filePosition = EEPROMReadLong(2);
}

void saveSongAndPositionInEeprom(long filePosition){
  if (filePosition==0){
    EEPROM.write(0, currentFolder);
    EEPROM.write(1, currentFile);
  }
  EEPROMWriteLong(2, filePosition);
  lastEepromEvent = millis();
}

// laedt einen long aus dem eeprom
// uebernommen aus http://playground.arduino.cc/Code/EEPROMReadWriteLong
long EEPROMReadLong(long address){
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

// speichert einen long in dem eeprom
// uebernommen aus http://playground.arduino.cc/Code/EEPROMReadWriteLong
void EEPROMWriteLong(int address, long value){
  // Decomposition from a long to 4 bytes by using bitshift.
  // One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);
 
  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

// Gibt den aktuellen Zustand fuer Debugzwecke aus
void printState(String location){
  println(location + 
  " - currentFolder:"+String(currentFolder)+
  ", button:"+String(button)+
  ", pause:"+String(paused)+
  ", time:"+String(millis())+"/"+String(lastButtonEvent)+
  ", filename:"+String(filename));
}

void println(String logMsg) {
  if (!debug) return;
  Serial.println(logMsg);
}
