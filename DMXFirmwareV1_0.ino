#include <Arduino.h>
#include <esp_dmx.h>
#include "Preferences.h"

TaskHandle_t DMXTask; //Multicore
TaskHandle_t OutputTask;

Preferences dmxprefs; //pref related
int startadress = 1; //DMX-Startadresse
int dmxmode = 1; //DMX-mode (RGBWAUV Strobe ETC)
short dimcurve[256]; //Dimmerkurve als Cache, um Berechnungn zu vermeiden

//DMX Setup
dmx_port_t dmxPort = 1; //serial port
byte data[DMX_PACKET_SIZE];
int transmitPin = 17;
int receivePin = 16;
int enablePin = 13;

byte dmx[DMX_PACKET_SIZE -1]; //nach evaluation des Startbytes

void setup() {
  Serial.begin(115200); 

  dmxprefs.begin("dmx-data", false); //read prefs before initializing driver
  startadress = dmxprefs.getUShort("startadress", 1);
  dmxmode = dmxprefs.getUShort("dmxmode", 1);
  Serial.println("STARTADRESS" + String(startadress));
  Serial.println("DMXMODE" + String(dmxmode));
  for (int i = 0; i <= 255; i++) {
    dimcurve[i] = dmxprefs.getUShort(("dimc"+String(i)).c_str()); //load dimmer curve to improve speed
  }

  //create a task that will be executed in the DMXcode() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    DMXcode,   /* Task function. */
                    "DMXTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &DMXTask,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //create a task that will be executed in the Outputcode() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Outputcode,   /* Task function. */
                    "OutputTask",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &OutputTask,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
    delay(500); 
}

void DMXcode( void * pvParameters ){
  Serial.print("DMX running on core ");
  Serial.println(xPortGetCoreID());

  dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

  dmx_driver_install(dmxPort, DMX_DEFAULT_INTR_FLAGS);

  for(;;){
    dmx_packet_t packet;
    if (dmx_receive(dmxPort, &packet, DMX_TIMEOUT_TICK)) {
      if (!packet.err) {
        dmx_read(dmxPort, data, packet.size);
        if (data[0] == 0) { //wenn startbyte 0 Daten kopieren und nicht weiter drum kümmern :)
          for (int i = 0; i < 512; i++) {
            dmx[i] = data[i +1];
          }
        }

      }
      else {
        Serial.println("DMX ERROR!");
      }
    }
  } 
}

void Outputcode( void * pvParameters ){
  Serial.print("OutputTask running on core ");
  Serial.println(xPortGetCoreID());

  // LED pins (PWM)
  const int led1r = 18;
  const int led1g = 19;
  const int led1b = 21;
  const int led1w = 22;

  const int led2r = 23;
  const int led2g = 12;
  const int led2b = 14;
  const int led2w = 27;

  const int led3r = 2;
  const int led3g = 4;
  const int led3b = 5;
  const int led3w = 15;

  const int led4r = 26;
  const int led4g = 25;
  const int led4b = 33;
  const int led4w = 32;

  for (int i = 0; i < 16; i++) {
    ledcSetup(i, 24000, 10); //24khz, ch 0, 10 bits
  }
  //Lampe 1
  ledcAttachPin(led1r, 0);
  ledcAttachPin(led1g, 1);
  ledcAttachPin(led1b, 2);
  ledcAttachPin(led1w, 3);

  //Lampe 2
  ledcAttachPin(led2r, 4);
  ledcAttachPin(led2g, 5);
  ledcAttachPin(led2b, 6);
  ledcAttachPin(led2w, 7);

  //Lampe 3
  ledcAttachPin(led3r, 8);
  ledcAttachPin(led3g, 9);
  ledcAttachPin(led3b, 10);
  ledcAttachPin(led3w, 11);

  //Lampe 4
  ledcAttachPin(led4r, 12);
  ledcAttachPin(led4g, 13);
  ledcAttachPin(led4b, 14);
  ledcAttachPin(led4w, 15);  

  boolean strobestatus1 = 0;
  unsigned long strobetmr1 = 0;

  boolean strobestatus2 = 0;
  unsigned long strobetmr2 = 0;

  boolean strobestatus3 = 0;
  unsigned long strobetmr3 = 0;

  boolean strobestatus4 = 0;
  unsigned long strobetmr4 = 0;

  for(;;){
    //Lampe 1
    if (dmx[startadress + 3] != 0) {
      if (millis() - strobetmr1 > 25 && strobestatus1 == 1) {
        ledcWrite(0, 0);
        ledcWrite(1, 0);
        ledcWrite(2, 0);
        ledcWrite(3, 0);
        strobetmr1 = millis();
        strobestatus1 = 0;
      }
      if (millis() - strobetmr1 > 4*(261-dmx[startadress + 3]) && strobestatus1 == 0) {
        ledcWrite(0, dimcurve[dmx[startadress -1]]);
        ledcWrite(1, dimcurve[dmx[startadress]]);
        ledcWrite(2, dimcurve[dmx[startadress + 1]]);
        ledcWrite(3, dimcurve[dmx[startadress + 2]]);
        strobetmr1 = millis();
        strobestatus1 = 1;
      }
    }
    else {
      ledcWrite(0, dimcurve[dmx[startadress -1]]);
      ledcWrite(1, dimcurve[dmx[startadress]]);
      ledcWrite(2, dimcurve[dmx[startadress + 1]]);
      ledcWrite(3, dimcurve[dmx[startadress + 2]]);
    }

  //Lampe 2
    if (dmx[startadress + 8] != 0) {
      if (millis() - strobetmr2 > 25 && strobestatus2 == 1) {
        ledcWrite(4, 0);
        ledcWrite(5, 0);
        ledcWrite(6, 0);
        ledcWrite(7, 0);
        strobetmr2 = millis();
        strobestatus2 = 0;
      }
      if (millis() - strobetmr2 > 4*(261-dmx[startadress + 8]) && strobestatus2 == 0) {
        ledcWrite(4, dimcurve[dmx[startadress + 4]]);
        ledcWrite(5, dimcurve[dmx[startadress + 5]]);
        ledcWrite(6, dimcurve[dmx[startadress + 6]]);
        ledcWrite(7, dimcurve[dmx[startadress + 7]]);
        strobetmr2 = millis();
        strobestatus2 = 1;
      }
    }
    else {
      ledcWrite(4, dimcurve[dmx[startadress + 4]]);
      ledcWrite(5, dimcurve[dmx[startadress + 5]]);
      ledcWrite(6, dimcurve[dmx[startadress + 6]]);
      ledcWrite(7, dimcurve[dmx[startadress + 7]]);
    }

  //Lampe 3
    if (dmx[startadress + 13] != 0) {
      if (millis() - strobetmr3 > 25 && strobestatus3 == 1) {
        ledcWrite(8, 0);
        ledcWrite(9, 0);
        ledcWrite(10, 0);
        ledcWrite(11, 0);
        strobetmr3 = millis();
        strobestatus3 = 0;
      }
      if (millis() - strobetmr3 > 4*(261-dmx[startadress + 13]) && strobestatus3 == 0) {
        ledcWrite(8, dimcurve[dmx[startadress + 9]]);
        ledcWrite(9, dimcurve[dmx[startadress + 10]]);
        ledcWrite(10, dimcurve[dmx[startadress + 11]]);
        ledcWrite(11, dimcurve[dmx[startadress + 12]]);
        strobetmr3 = millis();
        strobestatus3 = 1;
      }
    }
    else {
      ledcWrite(8, dimcurve[dmx[startadress + 9]]);
      ledcWrite(9, dimcurve[dmx[startadress + 10]]);
      ledcWrite(10, dimcurve[dmx[startadress + 11]]);
      ledcWrite(11, dimcurve[dmx[startadress + 12]]);
    }

  //Lampe 4
    if (dmx[startadress + 18] != 0) {
      if (millis() - strobetmr4 > 25 && strobestatus4 == 1) {
        ledcWrite(12, 0);
        ledcWrite(13, 0);
        ledcWrite(14, 0);
        ledcWrite(15, 0);
        strobetmr4 = millis();
        strobestatus4 = 0;
      }
      if (millis() - strobetmr4 > 4*(261-dmx[startadress + 18]) && strobestatus4 == 0) {
        ledcWrite(12, dimcurve[dmx[startadress + 14]]);
        ledcWrite(13, dimcurve[dmx[startadress + 15]]);
        ledcWrite(14, dimcurve[dmx[startadress + 16]]);
        ledcWrite(15, dimcurve[dmx[startadress + 17]]);
        strobetmr4 = millis();
        strobestatus4 = 1;
      }
    }
    else {
      ledcWrite(12, dimcurve[dmx[startadress + 14]]);
      ledcWrite(13, dimcurve[dmx[startadress + 15]]);
      ledcWrite(14, dimcurve[dmx[startadress + 16]]);
      ledcWrite(15, dimcurve[dmx[startadress + 17]]);
    }
  }
}

void loop() {
  
}
