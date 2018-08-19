/*
  Projekt: BWPBV
  Beschreibung: Target fuer Schiessstand
  Datum: 15.12.2015
  (ARDUINO UNO)
  ATMEL 328P

  Sensoreingänge PORT-B: PIN: 14, 15, 16, 17
  BlueTooth PIN:5 RX, 6 TX
  Reset: PIN 23
  LED: PIN 24

  Autor: M.HÃ¶ller-Schlieper

  Einstellungen für Programmer:
    ATMEL STK500 development board
    Arduino Uno

  - Kreisschnittpunkt berechnen
  - Korrektur: lineares Prüfen
  - Speichern des InfoStrings im EEProm

  Version 2051:
  10.10.2016 Kennzeichen: "VE"
  28.11.2016 Berechnungsalgorithmus in der APP

*/


#include <avr/io.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

SoftwareSerial BTSerial = SoftwareSerial(9, 10); //10 RX | 11 TX
//SoftwareSerial APC220Serial = SoftwareSerial(12, 13); //12 RX | 13 TX


//======================================
// Konstanten

const int MaxMicros = 4000;

const int Version = 2051;

// Schrittkette
const int skWarteAufSchwelle = 0;
const int skBerechneMaximum = 1;
const int skAuswertung = 2;
const int skReadSamples = 3;
const int skSendSamples = 4;
const int skWarteAufSchwelle_Neu = 5;
const int skSendSamples_Neu = 6;
const int skDebug = 10000;

const unsigned long c_TimeOutCounter = 100000;
const int iSensorPlattenBreite = 34;
// IOs
const int Sensor1 = 0;
const int Sensor2 = 1;
const int Sensor3 = 2;
const int Sensor4 = 3;

const int LED_Ready = 13;
const int RESET = 12;

const int LED_Sofair = 2;
const int LED_Nerf = 3;

// Anzahl der Samples
const int c_MaxSamples = 1200;

// Länge des Infostrings
const int lenInfoStr = 255;
const int c_notUsed = -1;
//======================================
// Strukturen
struct TSensor {
  double iLaufzeitMykroSec;
  double iLaufzeitMykroSecCalc;
  int iSensorNr;
  double dRadius;
  double dRadiusCalc;
};

//======================================
// Variablen

int Schritt = skWarteAufSchwelle_Neu;//skWarteAufSchwelle; //skDebug;//
unsigned long MikrosekStart = 0;
unsigned long AufnahmeDauer = 0;
unsigned long Micros = 0;
byte inputs[c_MaxSamples];
int ActValue;

String Infostr = String(lenInfoStr);

TSensor Sensoren[4];

long Counter = 0;
float iDiff = 0;

double iLaufzeitMykroSec = 0;
int iAnzTelegrammeEmpfangen = 0;

String str;
char c;

bool bDatenEmpfangen = false;
//=========================================
//=========================================
//=========================================
//=========================================

void ReadEEPROM() {
  // den Infostr aus EEPROM lesen
  int addr = 0;
  char c;

  Infostr = "";
  do {

    c = EEPROM.read(addr);
    Infostr += c;
    addr++;

  } while (c != '|' && addr < lenInfoStr);


}

void WriteEEPROM() {
  int addr = 0;
  char c;

  Infostr.trim();
  // ins EEPROM schreiben
  do {
    c = Infostr[addr];
    EEPROM.write(addr, c);
    addr++;
  } while (c != '|' && addr < lenInfoStr);
}

//=========================================
//  SendSamples - die Samples an Host senden
//=========================================

void SendSamples_Neu() {
  int addr = 0;

  //================================================
  // Bluetooth, USB, Funk
  //================================================
  if (BTSerial) {

    // Daten ins EEPROM schreiben...
    if (BTSerial.available() > 0)
    {
      digitalWrite(LED_Ready, LOW);

      delay(200);
      digitalWrite(LED_Ready, HIGH);

      delay(200);
      digitalWrite(LED_Ready, LOW);

      delay(200);
      digitalWrite(LED_Ready, HIGH);

      //while (BTSerial.available() == 0);

      char c = ' ';
      Infostr = "";

      // warte auf das Startsymbol "D"
      while (c != 'D') {
        if (BTSerial.available() > 0) {
          c = BTSerial.read();
        } else {
          delay(3);
        }
      }

      // solange bis Terminalsymbol kommt....
      while (c != '|') {
        if (BTSerial.available() > 0) {
          Infostr += c;
          c = BTSerial.read();
        } else {
          delay(3);
        }
      }

      // das Terminalsymbol anhängen
      Infostr += c;


      WriteEEPROM();
      ReadEEPROM();

      BTSerial.print(Infostr);
      BTSerial.println("END |"); // end of file

      delay(200);
      digitalWrite(LED_Ready, LOW);

      delay(200);
      digitalWrite(LED_Ready, HIGH);

      delay(200);
      digitalWrite(LED_Ready, LOW);

      delay(200);
      digitalWrite(LED_Ready, HIGH);


      BTSerial.flush();

    } else {

      BTSerial.flush();
      BTSerial.print("KOORD ");
      BTSerial.print(Version);// Versionsnummer
      BTSerial.print(" ");
      BTSerial.print(-1); // Kennung, dass hier die XY-Position nicht berechnet wurde
      BTSerial.print(" ");
      BTSerial.print(iSensorPlattenBreite);
      BTSerial.print(" ");
      BTSerial.print(Sensoren[0].iLaufzeitMykroSec);
      BTSerial.print(" ");
      BTSerial.print(Sensoren[1].iLaufzeitMykroSec);
      BTSerial.print(" ");
      BTSerial.print(Sensoren[2].iLaufzeitMykroSec);
      BTSerial.print(" ");
      BTSerial.print(Sensoren[3].iLaufzeitMykroSec);
      BTSerial.print(" ");
      BTSerial.print(iDiff);
      BTSerial.print(" ");
      BTSerial.print(Counter);
      BTSerial.print(" ");
      BTSerial.print(0);
      BTSerial.print(" ");
      BTSerial.print("I");
      BTSerial.print(" ");
      BTSerial.print(Infostr);
      BTSerial.print(" ");
      BTSerial.println("|"); // end of file

    }
  }
}
//=========================================
// Setup
//=========================================
void setup() {
  // initialisiere die seriellen Ports
  //Serial.begin(115200);
  //Serial.flush();

  /*  Serial1.begin(9600);
    Serial1.flush();
  */

  BTSerial.begin(9600);
  BTSerial.flush();
  BTSerial.println("BT Connected");
  BTSerial.listen();

  /* APC220Serial.begin(9600);
    APC220Serial.flush();
    APC220Serial.println("APC Connected");
  */
  // setze die Konfiguration an PORT - D
  // FÜR den MEGA an PORT-D: PIN 2, 3, 4, 5  Sensoreingänge
  // nur lesen an Port D
  DDRD = 0b00000000;


  pinMode(LED_Ready, OUTPUT);
  pinMode(RESET, OUTPUT);

  // Ready LED EIN
  digitalWrite(LED_Ready, LOW);
  delay(200);
  digitalWrite(RESET, LOW);
  delay(200);
  digitalWrite(RESET, HIGH);

  ReadEEPROM();

  if (!Infostr.startsWith("D ")) {
    Infostr = "NO DATA |";
  }
}

//=========================================
//=========================================
//=========================================
//=========================================
//=========================================
// Hauptprogramm
//=========================================
//=========================================
//=========================================
//=========================================
//=========================================

void loop() {

  //===========================================================
  switch (Schritt) {

    case skWarteAufSchwelle_Neu:

      // leere das Feld
      for (int i = 0; i < c_MaxSamples; i++) {
        inputs[i] = 0;
      }

      Counter = 0;

      //==================================================
      // solange alle Eingänge 0 sind
      //==================================================
      noInterrupts(); // Turbo einschalten
      while ((PIND & 0x0F)  == 0); // warte wis ein Eingang gesetzt wurde
      TCNT2 = 0;
      Micros = TCNT2; // die Startzeit merken
      do {
        //==================================================
        // lese die Eingänge und schreibe in das Array
        //==================================================
        inputs[Counter++] = (byte)PIND;
      } while (Counter < c_MaxSamples);

      iDiff = (TCNT2 - Micros); // die gemessene Zeit
      interrupts();  // Turbo ausschalten und Interrupts ein

      //==================================================
      // die Auflösung berechnen
      //==================================================
      if (Counter > 0 && iDiff > 0) {
        // die Auflösung berechnen
        iDiff = iDiff / Counter;

        //==================================================
        // die Flanken aus dem Array lesen...
        //==================================================

        Sensoren[0].iLaufzeitMykroSec = c_notUsed;
        Sensoren[1].iLaufzeitMykroSec = c_notUsed;
        Sensoren[2].iLaufzeitMykroSec = c_notUsed;
        Sensoren[3].iLaufzeitMykroSec = c_notUsed;

        for (long i = 0; i < Counter; i++) {
          // ((0.06 / iDiff) * (i * iDiff)) / 1000;
          iLaufzeitMykroSec = i;

          if (bitRead(inputs[i], 0) && Sensoren[0].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[0].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }

          if (bitRead(inputs[i], 1) && Sensoren[1].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[1].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }

          if (bitRead(inputs[i], 2) && Sensoren[2].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[2].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }

          if (bitRead(inputs[i], 3) && Sensoren[3].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[3].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }
        }

        // berechne die XY-Position
        Schritt = skSendSamples_Neu;

      }

      break;
    //=========================================
    case skSendSamples_Neu:
      // READY LED EIN
      digitalWrite(LED_Ready, HIGH);
      // entprelle das System
      // die Sensoren zurücksetzen
      digitalWrite(RESET, LOW);

      // Sampels senden
      SendSamples_Neu();
      delay(50);

      // warte bis die Eingänge wirklich zurückgesetzt sind
      while ((PIND & 0x0F)  != 0);

      delay(20);
      digitalWrite(RESET, HIGH);
      digitalWrite(LED_Ready, LOW);

      // warte auf Schwelle
      Schritt = skWarteAufSchwelle_Neu;
      break;

  }
}

