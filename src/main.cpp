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
    <p>Brightness: <input type="range" min="0" max="4095" oninput="fetch('/brightness?val='+this.value)"></p>
    <p>Color: <input type="color" oninput="fetch('/color?val=' + encodeURIComponent(this.value))"></p>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", page);
}

void spiSendBytes(const uint8_t* data, size_t length)
{
  SPI.beginTransaction(spiSettings);

  digitalWrite(SS, LOW);

  SPI.transferBytes(data, nullptr, length);

  digitalWrite(SS, HIGH);
  
  SPI.endTransaction();
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
uint8_t LED_digital_write(uint8_t LED_id, uint8_t state)
{
  // MSG = 0b000A AAAV
  // AAAA = LED ID (0-15)
  // V = Value (0=OFF, 1=ON)
  if(LED_id > 15) return 0x01;
  uint8_t data = 0b00000000;
  data = data | LED_id;
  data = data | ((state & 0x01) << 4);
  spiSend(data);
  return 0x00;
}
uint8_t LED_PWM_write(uint8_t LED_id, uint16_t brightness)
{
  // MSG = 0b0010AAAA BBBBBBBB BBBB0000 
  if(LED_id > 15) return 0x01;
  if(brightness > 4095) brightness = 4095;
  uint8_t data[3];
  data[0] = 0b00100000 | (LED_id);
  data[1] = (brightness >> 4) & 0xFF; // High byte
  data[2] = (brightness << 4) & 0xFF; // Low byte
  spiSendBytes(data, 3);
  return 0x00;
}
uint8_t LED_RGB_write(uint8_t LED_id, uint16_t red, uint16_t green, uint16_t blue)
{
  // MSG = 0b0100aaaa 0brrrrrrrr 0brrrrgggg 0bgggggggg 0b_bbbbbbbb 0b_bbbb0000
  if(LED_id > 15) return 0x01;
  uint8_t data[6];
  data[0] = 0b01000000 | (LED_id);
  data[1] = (red >> 4) & 0xFF;   // Extract high 8 bits
  data[2] = 0b00000000 | ((red << 4) & 0xF0) | ((green >> 8) & 0x0F); // Combine low 4 bits of red and high 4 bits of green
  data[3] = green & 0xFF; // Low 8 bits of green
  data[4] = (blue >> 4) & 0xFF;  // Extract high 8 bits of blue
  data[5] = (blue << 4) & 0xF0; // Low 4 bits of blue, last 4 bits are padding
  spiSendBytes(data, 6);
  return 0x00;
}
uint8_t LED_config(uint8_t RGB_LED_count, uint8_t greyscale_LED_count)
{
  // MSG = 0b_01100000  0b_mmmnnnnn
  if(RGB_LED_count > 5) return 0x01;
  if(greyscale_LED_count > 15) return 0x02;
  uint8_t data[2];
  data[0] = 0b01100000;
  data[1] = (0b11100000 & (RGB_LED_count << 5)) | (0b00011111 & greyscale_LED_count);
  spiSendBytes(data, 2);
  return 0x00;
}
void turnLEDOn()
{
  Serial.println("LED was turned ON");
  isLEDOn = true;
  LED_digital_write(0, 1);
  // spiSend(0b0001111);
}
void turnLEDOff()
{
  Serial.println("LED was turned OFF");
  isLEDOn = false;
  LED_digital_write(0, 0);
  // spiSend(0b11110000);
}

void setup()
{
  Serial.begin(115200);
  isLEDOn = false;
  WiFi.softAP("ESP8266 LED Dimmer", "YOLOisTOOshort");
  server.on("/", handleRoot);
  server.on("/on", turnLEDOn);
  server.on("/off", turnLEDOff);
  server.on("/brightness", []() {
    if (server.hasArg("val")) {
      int brightness = server.arg("val").toInt();
      brightness = constrain(brightness, 0, 4095);
      Serial.print("Setting brightness to: ");
      Serial.println(brightness);

      LED_PWM_write(0, brightness);
      
      server.send(200, "text/plain", "Brightness set to " + String(brightness));
    } else {
      server.send(400, "text/plain", "Bad Request: 'val' parameter missing");
    }
  });
  server.on("/color", HTTP_GET, []() {
    if (server.hasArg("val")) {
      String colorStr = server.arg("val");
      Serial.print("Received color value: ");
      Serial.println(colorStr);
      Serial.print("Raw server.arg: ");
      Serial.println(server.arg("val"));
      if(colorStr.length() == 7 && colorStr.charAt(0) == '#') {
        long color = strtol(colorStr.substring(1).c_str(), NULL, 16);
        uint16_t red = (color >> 16) & 0xFF;
        uint16_t green = (color >> 8) & 0xFF;
        uint16_t blue = color & 0xFF;
        red = map(red, 0, 255, 0, 4095);
        green = map(green, 0, 255, 0, 4095);
        blue = map(blue, 0, 255, 0, 4095);
        Serial.print("Setting color to R:");
        Serial.print(red);
        Serial.print(" G:");
        Serial.print(green);
        Serial.print(" B:");
        Serial.println(blue);

        //LED_RGB_write(0, red, green, blue);
        
        server.send(200, "text/plain", "Color set to " + colorStr);
      } else {
        server.send(400, "text/plain", "Bad Request: 'val' parameter invalid");
      }
    } else {
      server.send(400, "text/plain", "Bad Request: 'val' parameter missing");
    }
  });
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