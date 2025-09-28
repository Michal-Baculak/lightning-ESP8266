/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/vs-code-platformio-ide-esp32-esp8266-arduino/
*********/

#include <Arduino.h>
#include <ESP8266WiFi.h>

#define LED 5

const char *ssid = "ESP8266 AP";
const char *password = "YOLOisTOOshort";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  boolean result = WiFi.softAP(ssid, password);
  if(result)
    Serial.println("Access point started!");
  else
    Serial.println("Access point failed to start!");
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Network is operating under SSID: ");
  Serial.println(WiFi.softAPSSID());
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(LED, HIGH);
  Serial.println("LED is on");
  delay(1000);
  digitalWrite(LED, LOW);
  Serial.println("LED is off");
  delay(1000);
  Serial.printf("Connected stations: %d\n",WiFi.softAPgetStationNum());
}