#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <SPI.h>

ESP8266WebServer server(80);
static const int spiCLKFreq = 1e6; //1MHz clock
SPISettings spiSettings(spiCLKFreq, MSBFIRST, SPI_MODE0);


int rgbCount = 0;
int grayCount = 0;

void testSequence(void);

void spiSend(uint8_t byte)
{
  SPI.beginTransaction(spiSettings);
  digitalWrite(SS, LOW);
  delayMicroseconds(5);

  uint8_t status = SPI.transfer(byte);
  
  delayMicroseconds(5);
  digitalWrite(SS, HIGH);
  SPI.endTransaction();

  Serial.print("SPI sent data resulting in status: ");
  Serial.println(status);
}

void spiSendBytes(const uint8_t* data, size_t length)
{

  for (size_t i = 0; i < length; i++)
  {
    spiSend(data[i]);
    delayMicroseconds(5);
  }
}

uint8_t spiReadByte()
{
  SPI.beginTransaction(spiSettings);
  digitalWrite(SS, LOW);

  uint8_t value = SPI.transfer(0xFF); // dummy byte (0xFF) is unused by the convention

  digitalWrite(SS, HIGH);
  SPI.endTransaction();

  return value;
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
uint8_t LED_RGB_write(uint8_t LED_id, uint8_t red, uint8_t green, uint8_t blue)
{
  // MSG = 0b0100aaaa 0brrrrrrrr 0bgggggggg 0bbbbbbbbb
  Serial.print("LED_RGB_write called with ID: ");
  Serial.println(LED_id);
  Serial.print("Red: ");
  Serial.println(red);
  Serial.print("Green: ");
  Serial.println(green);
  Serial.print("Blue: ");
  Serial.println(blue);
  if(LED_id > 5) return 0x01;
  uint8_t data[4];
  data[0] = 0b01000000 | (LED_id);
  data[1] = red;
  data[2] = green;
  data[3] = blue;
  spiSendBytes(data, 4);
  Serial.println("SPI command for RGB LED sent.");
  Serial.print("SPI data 0: ");
  Serial.println(data[0], BIN);
  Serial.print("SPI data 1: ");
  Serial.println(data[1], BIN);
  Serial.print("SPI data 2: ");
  Serial.println(data[2], BIN);
  Serial.print("SPI data 3: ");
  Serial.println(data[3], BIN);
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

  // wait for slave to perform its starting sequence - 300ms per pin, which is 300*16 = 4800ms max
  Serial.println("Config sent to slave, waiting for execution...");
  unsigned long slave_await_delay = 300*(RGB_LED_count * 3 + greyscale_LED_count + 1);
  delay(slave_await_delay);

  // poll slave for response 10 times with 100ms intervals
  Serial.println("Polling slave for response...");
  for (size_t i = 0; i < 10; i++)
  {
    uint8_t slave_response = spiReadByte();
    Serial.printf("Slave responded with message: %d", slave_response);
    // correct response is 0b1
    if(slave_response == 0b1)
      return 0x00;

    delay(100);
  }
  Serial.printf("Serial polling timed out, error");
  return 0x01; // 0x01: error - slave timeout
}

void handleRoot()
{
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP8266 LED Control</title>
    <style>
      body { font-family: Arial; text-align: center; margin: 20px; }
      button { padding: 10px 20px; margin: 5px; }
      .led-panel { border: 1px solid #ccc; border-radius: 10px; padding: 10px; margin: 10px; display: inline-block; }
      .settings { margin-top: 20px; }
    </style>
  </head>
  <body>
    <h1>ESP8266 LED Control</h1>
    <button onclick="showSettings()">Settings</button>
    <div id="settings" class="settings" style="display:none;">
      <h3>Configuration</h3>
      <p>
        RGB LEDs: 
        <select id="rgbCount">
          <option>0</option><option>1</option><option>2</option><option>3</option><option>4</option>
        </select>
      </p>
      <p>
        Greyscale LEDs: 
        <select id="grayCount">
          <option>0</option><option>1</option><option>2</option><option>3</option><option>4</option>
          <option>5</option><option>6</option><option>7</option><option>8</option><option>9</option>
          <option>10</option><option>11</option><option>12</option><option>13</option><option>14</option>
        </select>
      </p>
      <button onclick="saveConfig()">Save</button>
    </div>

    <div id="ledPanels"></div>

    <script>
      async function showSettings() {
        const res = await fetch('/getConfig');
        const text = await res.text(); // example: "rgb=2&gray=3"
        const params = new URLSearchParams(text);
        document.getElementById('rgbCount').value = params.get('rgb');
        document.getElementById('grayCount').value = params.get('gray');
        document.getElementById('settings').style.display = 'block';
      }

      async function saveConfig() {
        const rgb = document.getElementById('rgbCount').value;
        const gray = document.getElementById('grayCount').value;
        await fetch(`/saveConfig?rgb=${rgb}&gray=${gray}`);
        buildPanels(rgb, gray);
        document.getElementById('settings').style.display = 'none';
      }

      async function buildPanels(rgbCount, grayCount) {
        // If not provided, fetch from ESP
        if (!rgbCount || !grayCount) {
          const res = await fetch('/getConfig');
          const text = await res.text();
          const params = new URLSearchParams(text);
          rgbCount = params.get('rgb');
          grayCount = params.get('gray');
        }

        let html = '';
        const rgb = parseInt(rgbCount);
        const gray = parseInt(grayCount);

        for (let i = 1; i <= rgb; i++) {
          html += `
            <div class="led-panel">
              <h3>RGB LED ${i}</h3>
              <button onclick="fetch('/rgb${i}/on')">ON</button>
              <button onclick="fetch('/rgb${i}/off')">OFF</button>
              <p>Color: <input type="color" oninput="fetch('/rgb${i}/color?val='+encodeURIComponent(this.value))"></p>
            </div>`;
        }

        for (let i = 1; i <= gray; i++) {
          html += `
            <div class="led-panel">
              <h3>Greyscale LED ${i}</h3>

              <button onclick="
                fetch('/gray${i}/on');
                document.getElementById('gray-slider-${i}').value = 4095;
              ">ON</button>

              <button onclick="
                fetch('/gray${i}/off');
                document.getElementById('gray-slider-${i}').value = 0;
              ">OFF</button>

              <p>
                Brightness:
                <input
                  type="range"
                  id="gray-slider-${i}"
                  min="0"
                  max="4095"
                  value="0"
                  oninput="fetch('/gray${i}/brightness?val=' + this.value)"
                >
              </p>
            </div>`;
        }

        document.getElementById('ledPanels').innerHTML = html;
      }

      window.onload = buildPanels;
    </script>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", page);
}

void handleGetConfig() {
  String response = "rgb=" + String(rgbCount) + "&gray=" + String(grayCount);
  server.send(200, "text/plain", response);
}

void handleSaveConfig() {
  if (server.hasArg("rgb")) rgbCount = server.arg("rgb").toInt();
  if (server.hasArg("gray")) grayCount = server.arg("gray").toInt();
  Serial.printf("New config: RGB=%d, Gray=%d\n", rgbCount, grayCount);
  
  //send command over SPI
  LED_config(rgbCount, grayCount);

  server.send(200, "text/plain", "OK");
}

void handleEndpoint()
{
  Serial.print("Received request: ");
  String uri = server.uri();
  String val = server.arg("val");
  Serial.println(uri);

  if (uri.startsWith("/rgb")) 
  {
    int numStart = 4;
    int numEnd = uri.indexOf('/', numStart);
    int ledNum = uri.substring(numStart, numEnd).toInt();
    int ledID = ledNum-1;
    String action = uri.substring(numEnd + 1);

    if (action == "on")
    {
      Serial.printf("RGB %d ON\n", ledNum);
      LED_digital_write(ledID,1);
    } 
    else if (action == "off") 
    {
      Serial.printf("RGB %d OFF\n", ledNum);
      LED_digital_write(ledID,0);
    }
    else if (action == "color")
    {
      Serial.printf("RGB %d COLOR %s\n", ledID, val.c_str());

      // check color format
      if(val.length() != 7 || val.charAt(0) != '#') 
      {
        Serial.println("Color argument is ill formatted!");
        server.send(200, "text/plain", "INCORRECT COLOR FORMAT");
        return;
      }

      // parse color
      long color = strtol(val.substring(1).c_str(), NULL, 16);
      uint16_t red = (color >> 16) & 0xFF;
      uint16_t green = (color >> 8) & 0xFF;
      uint16_t blue = color & 0xFF;

      // send command over SPI
      LED_RGB_write(ledID, red, green, blue);
    }
  }
  else if (uri.startsWith("/gray")) {
    int numStart = 5;
    int numEnd = uri.indexOf('/', numStart);
    int ledNum = uri.substring(numStart, numEnd).toInt();
    int ledID = ledNum - 1 + rgbCount;
    String action = uri.substring(numEnd + 1);

    if (action == "on")
    {
      Serial.printf("GRAY %d ON\n", ledNum);
      LED_digital_write(ledID, 1);
    } 
    else if (action == "off")
    {
      Serial.printf("GRAY %d OFF\n", ledNum);
      LED_digital_write(ledID, 0);
    }
    else if (action == "brightness")
    {
      Serial.printf("GRAY %d BRIGHTNESS %s\n", ledNum, val.c_str());
      int brightness = strtol(val.c_str(), NULL, 10);
      Serial.printf("Setting led ID: %d, to brightness %d", ledID, brightness);
      LED_PWM_write(ledID, brightness);
    } 
  }
  server.send(200, "text/plain", "OK");
}

void setup()
{
  Serial.begin(115200);
  WiFi.softAP("ESP8266 LED Dimmer", "YOLOisTOOshort");
  server.onNotFound(handleEndpoint);
  server.on("/", handleRoot);
  server.on("/getConfig", handleGetConfig);
  server.on("/saveConfig", handleSaveConfig);
  server.begin();
  Serial.println("Server is up and running");
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
  delay(1000);

  // testSequence();
}

void loop()
{
  server.handleClient();
}

void testSequence()
{
  Serial.println("Setting LED config to (2,3)");
  LED_config(2, 3);
  Serial.println("100ms delay...");
  delay(100); 
  Serial.println("Starting to gradually control LED 2");

  for(uint16_t i = 0; i < 4095; ++i)
  {
    LED_PWM_write(2, i);
    // delay(50);
  }
  for(uint16_t i = 4095; i >= 0; --i)
  {
    LED_PWM_write(2, i);
    // delay(50);
  }
  Serial.println("Sequence finished!");
}