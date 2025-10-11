#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <SPI.h>

#define LED_PIN 5

ESP8266WebServer server(80);
boolean isLEDOn;
static const int spiCLKFreq = 1e6; //1MHz clock
SPISettings spiSettings(spiCLKFreq, MSBFIRST, SPI_MODE0);

void handleRoot()
{
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP8266 Web server</title>
    <style>
      body { font-family: Arial; text-align: center; }
      button { padding: 10px 20px; margin: 5px; }
    </style>
  </head>
  <body>
    <h1>ESP8266 LED Control</h1>
    <button onclick="fetch('/on')">LED ON</button>
    <button onclick="fetch('/off')">LED OFF</button>
    <p>Brightness: <input type="range" min="0" max="255" oninput="fetch('/brightness?val='+this.value)"></p>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", page);
}
void spiSend(uint8_t byte)
{
  SPI.beginTransaction(spiSettings);

  digitalWrite(SS, LOW);

  uint8_t status = SPI.transfer(byte);

  digitalWrite(SS, HIGH);
  
  SPI.endTransaction();

  Serial.print("SPI sent data resulting in status: ");
  Serial.println(status);
}
void turnLEDOn()
{
  Serial.println("LED was turned ON");
  isLEDOn = true;
  spiSend(0b0001111);
}
void turnLEDOff()
{
  Serial.println("LED was turned OFF");
  isLEDOn = false;
  spiSend(0b11110000);
}

void setup()
{
  Serial.begin(115200);
  isLEDOn = false;
  WiFi.softAP("ESP8266 LED Dimmer", "YOLOisTOOshort");
  server.on("/", handleRoot);
  server.on("/on", turnLEDOn);
  server.on("/off", turnLEDOff);
  server.begin();
  Serial.println("Server is up and running");
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Printing SPI interface pins: ");
  Serial.print("MISO: ");
  Serial.println(MISO);
  Serial.print("MOSI: ");
  Serial.println(MOSI);
  Serial.print("SCK: ");
  Serial.println(SCK);
  Serial.print("SS: ");
  Serial.println(SS);
  SPI.begin();
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
}

void loop()
{
  server.handleClient();
  if(isLEDOn)
    digitalWrite(LED_PIN, HIGH);
  else
    digitalWrite(LED_PIN, LOW);
}