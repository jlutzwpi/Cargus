//
// Copyright 2019 Blues Inc.  All rights reserved.
// Use of this source code is governed by licenses granted by the
// copyright holder including that found in the LICENSE file.
//
// This example contains the complete source for the Sensor Tutorial at dev.blues.io
// https://dev.blues.io/build/tutorials/sensor-tutorial/notecarrier-af/esp32/arduino-wiring/
//
// This tutorial requires an external Adafruit BME680 Sensor.
//

// Include the Arduino library for the Notecard
#include <Notecard.h>

#include <Wire.h>
#include <Arduino.h>
#include "I2CDriver.h"

//#define serialNotecard Serial1
#define serialDebug Serial

#define productUID "edu.wpi.alum.jlutz:baby_on_board"
Notecard notecard;
I2CDriver i2c = I2CDriver();
//results struct from the useful sensor
inference_results_t results;
bool textSent = false;

// One-time Arduino initialization
void setup()
{
#ifdef serialDebug
    delay(2500);
    serialDebug.begin(115200);
    notecard.setDebugOutputStream(serialDebug);
#endif

    // Initialize the physical I/O channel to the Notecard
#ifdef serialNotecard
    notecard.begin(serialNotecard, 9600);
#else
    Wire.begin();

    notecard.begin();
#endif
    //set cellular to periodic update every 2 minutes
    //can't have GPS and cell be continous at same time
    J *req = notecard.newRequest("hub.set");
    JAddStringToObject(req, "product", productUID);
    JAddStringToObject(req, "mode", "periodic");
    JAddNumberToObject(req, "outbound", 3);
    JAddNumberToObject(req, "inbound", 720);
    notecard.sendRequest(req);

    //set GPS to continuous
    req = notecard.newRequest("card.location.mode");
    JAddStringToObject(req, "mode", "periodic");
    JAddNumberToObject(req, "seconds", 15);
    notecard.sendRequest(req);
  
    req = notecard.newRequest("card.location.track");
    JAddBoolToObject(req, "sync", true);
    JAddBoolToObject(req, "heartbeat", true);
    //JAddNumberToObject(req, "hours", 1);
    notecard.sendRequest(req);

    i2c.begin();
    i2c.setMode(i2c.MODE_CONTINUOUS);
    i2c.setIdModelEnabled(true);
    i2c.setDebugMode(true);
    i2c.setPersistentIds(false);

    pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
    //get results from sensor
    results = i2c.read();
    if(results.num_faces == 0)
      digitalWrite(LED_BUILTIN, LOW);
    for (int i=0; i< results.num_faces; i++) {
      uint8_t confidence = results.boxes[i].confidence;
      if(confidence > 99) confidence = 99;
      //face ID'd
      if(confidence > 70)
      {
        //digitalWrite(LED_BUILTIN, HIGH); 
        uint8_t id_confidence = results.boxes[i].id_confidence;
        int8_t id = results.boxes[i].id;
        Serial.print("Face detected!  Confidence: ");
        Serial.println(confidence);
        int motionTime = 0;
        int cardTime = 0;
        //get time of last motion
        J *req = notecard.requestAndResponse(notecard.newRequest("card.motion"));
        if (req != NULL) {
          motionTime = JGetNumber(req, "motion");
          notecard.deleteResponse(req);
        }
        //get current time
        req = notecard.requestAndResponse(notecard.newRequest("card.time"));
        if (req != NULL) {
          cardTime = JGetNumber(req, "time");
          notecard.deleteResponse(req);
        }
        Serial.print("Time delta since last movement: ");
        int delta = cardTime - motionTime;
        Serial.print((float(delta) / 60.0));
        Serial.println(" minutes.");
        delay(20000); 
        //if current time is > 2 minutes from last motion, send text alert (ie baby is detected, but car is not moving)
        if(delta > 120 && textSent == false)
        {
          req = notecard.newRequest("note.add");
          if (req != NULL) {
              JAddStringToObject(req, "file", "twilio.qo");
              JAddBoolToObject(req, "sync", true);
              J *body = JCreateObject();
              if (body != NULL) {
                  JAddStringToObject(body, "customMessage", "Check on child! No movement in 5 minutes.");
                  JAddStringToObject(body, "customTo", "+15083080654");
                  JAddItemToObject(req, "body", body);
              }
              notecard.sendRequest(req);
              delay(2000);
          }
          textSent = true;
        }
        //sent another reminder at 10 minutes
        if(delta > 600)
          textSent = false;
      }
}
