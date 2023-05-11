// This code works with Joker8 and Seeed Studio XIAO ESP32C3

// Joker8 - XIAO GPIO assignment
//         [10][ 9][ 8]
// [ 5][ 6][ 7][21][20]

#include <WiFi.h>
#include "HeosControl.h"
#include "LgtvControl.h"

// Please modify ssid, password, heosdevice and lgtv.
const char* ssid     = "SSID";
const char* password = "PASSWORD";
const IPAddress heosdevice(192,168,1,40);
const IPAddress lgtv(192,168,1,41);

HeosControl hc;
LgtvControl lc;

volatile int g_macroId = 0; // 0 : invalid
String g_clientkey = "";

void macro1(){ g_macroId = 1; }
void macro2(){ g_macroId = 2; }
void macro3(){ g_macroId = 3; }
void macro4(){ g_macroId = 4; }
void macro5(){ g_macroId = 5; }
void macro6(){ g_macroId = 6; }
void macro7(){ g_macroId = 7; }
void macro8(){ g_macroId = 8; }

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.print("WiFi connected\r\n");

  pinMode(10, INPUT_PULLUP);
  pinMode( 9, INPUT_PULLUP);
  pinMode( 8, INPUT_PULLUP);
  pinMode( 5, INPUT_PULLUP);
  pinMode( 6, INPUT_PULLUP);
  pinMode( 7, INPUT_PULLUP);
  pinMode(21, INPUT_PULLUP);
  pinMode(20, INPUT_PULLUP);

  // Use ONLOW instead of FALLING because some switches caused unwanted interrupt on release.
  attachInterrupt(digitalPinToInterrupt(10), macro1, ONLOW);
  attachInterrupt(digitalPinToInterrupt( 9), macro2, ONLOW);
  attachInterrupt(digitalPinToInterrupt( 8), macro3, ONLOW);
  attachInterrupt(digitalPinToInterrupt( 5), macro4, ONLOW);
  attachInterrupt(digitalPinToInterrupt( 6), macro5, ONLOW);
  attachInterrupt(digitalPinToInterrupt( 7), macro6, ONLOW);
  attachInterrupt(digitalPinToInterrupt(21), macro7, ONLOW);
  attachInterrupt(digitalPinToInterrupt(20), macro8, ONLOW);

}

void loop() {
  if(g_macroId == 0){
    return;
  }

  switch(g_macroId){
    case 1:
    {
      if(!hc.Connect(heosdevice, true)){
        Serial.printf("(HEOS)Connection failed\r\n");
        break;
      }
      hc.SetVolume(20);
      hc.Disconnect();
      break;
    }
    case 2:
    {
      if(!hc.Connect(heosdevice, true)){
        Serial.printf("(HEOS)Connection failed\r\n");
        break;
      }
      hc.PlayInputSource(HeosControl::INPUT_SOURCE::USBDAC);
      hc.SetVolume(25);
      hc.Disconnect();
      break;
    }
    case 3:
    {
      if(!hc.Connect(heosdevice, true)){
        Serial.printf("(HEOS)Connection failed\r\n");
        break;
      }
      hc.PlayInputSource(HeosControl::INPUT_SOURCE::OPTICAL_IN_1);
      hc.SetVolume(30);
      hc.Disconnect();
      break;
    }
    case 4:
    {
      if(!lc.Connect(lgtv, g_clientkey)){
        Serial.printf("(LGTV)Connection failed\r\n");
        break;
      }
      lc.SwitchInput(LgtvControl::InputId::HDMI1);
      g_clientkey = lc.GetClientKey();
      lc.Disconnect();
      break;
    }
    case 5:
    {
      if(!lc.Connect(lgtv, g_clientkey)){
        Serial.printf("(LGTV)Connection failed\r\n");
        break;
      }
      lc.SwitchInput(LgtvControl::InputId::HDMI2);
      g_clientkey = lc.GetClientKey();
      lc.Disconnect();
      break;
    }
    case 6:
    {
      if(!lc.Connect(lgtv, g_clientkey)){
        Serial.printf("(LGTV)Connection failed\r\n");
        break;
      }
      lc.SwitchInput(LgtvControl::InputId::HDMI3);
      g_clientkey = lc.GetClientKey();
      lc.Disconnect();
      break;
    }
    case 7:
    {
      if(!lc.Connect(lgtv, g_clientkey)){
        Serial.printf("(LGTV)Connection failed\r\n");
        break;
      }
      lc.SwitchInput(LgtvControl::InputId::HDMI4);
      g_clientkey = lc.GetClientKey();
      lc.Disconnect();
      break;
    }
    default: break;
  }

  delay(100);
  g_macroId = 0;
}
