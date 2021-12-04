// EPD-Photo: Photo Frame using E-Paper
// 2021-11-28  T. Nakagawa

#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <soc/rtc_cntl_reg.h>
#include "EPDClass.h"

extern "C" int rom_phy_get_vdd33();

constexpr int WIDTH = 1024;
constexpr int HEIGHT = 758;
constexpr int PIN_SPV = 32;
constexpr int PIN_CKV = 27;
constexpr int PIN_MODE = 26;
constexpr int PIN_STL = 25;
constexpr int PIN_OE = 23;
constexpr int PIN_LE = 22;
constexpr int PIN_CL = 21;
constexpr int PIN_D0 = 12;
constexpr int PIN_DCDC = 33;
constexpr float SHUTDOWN_VOLTAGE = 2.7f;
constexpr int TOUCH_THRESHOLD = 30;
constexpr uint8_t font[16][16] = {	// Font data for the messages "CONF", "TRAN", "DONE" and "BATT".
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x1c, 0x18, 0x62, 0x7e, 0x7e, 0x7c, 0x18, 0x62, 0xf8, 0x18, 0x62, 0x7e, 0x78, 0x18, 0x7e, 0x7e},
  {0x26, 0x24, 0x62, 0x40, 0x10, 0x46, 0x18, 0x62, 0x44, 0x24, 0x62, 0x40, 0x46, 0x18, 0x10, 0x10},
  {0x42, 0x42, 0x62, 0x40, 0x10, 0x42, 0x18, 0x62, 0x46, 0x42, 0x62, 0x40, 0x42, 0x18, 0x10, 0x10},
  {0x40, 0x42, 0x52, 0x40, 0x10, 0x42, 0x24, 0x52, 0x42, 0x42, 0x52, 0x40, 0x42, 0x24, 0x10, 0x10},
  {0x40, 0x42, 0x52, 0x40, 0x10, 0x46, 0x24, 0x52, 0x42, 0x42, 0x52, 0x40, 0x44, 0x24, 0x10, 0x10},
  {0x40, 0x42, 0x5a, 0x7c, 0x10, 0x7c, 0x24, 0x5a, 0x42, 0x42, 0x5a, 0x7c, 0x78, 0x24, 0x10, 0x10},
  {0x40, 0x42, 0x4a, 0x40, 0x10, 0x48, 0x3c, 0x4a, 0x42, 0x42, 0x4a, 0x40, 0x44, 0x3c, 0x10, 0x10},
  {0x42, 0x42, 0x4a, 0x40, 0x10, 0x4c, 0x42, 0x4a, 0x42, 0x42, 0x4a, 0x40, 0x42, 0x42, 0x10, 0x10},
  {0x42, 0x42, 0x46, 0x40, 0x10, 0x44, 0x42, 0x46, 0x46, 0x42, 0x46, 0x40, 0x42, 0x42, 0x10, 0x10},
  {0x26, 0x24, 0x46, 0x40, 0x10, 0x44, 0x42, 0x46, 0x44, 0x24, 0x46, 0x40, 0x46, 0x42, 0x10, 0x10},
  {0x1c, 0x18, 0x46, 0x40, 0x10, 0x46, 0xc3, 0x46, 0xf8, 0x18, 0x46, 0x7e, 0x7c, 0xc3, 0x10, 0x10},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

Preferences preferences;
EPDClass epd(PIN_SPV, PIN_CKV, PIN_MODE, PIN_STL, PIN_OE, PIN_LE, PIN_CL, PIN_D0, PIN_DCDC);
RTC_DATA_ATTR int photo_counter;

// Manage the photo counter.
class Photo {
public:
  Photo(int counter) : counter_(counter) {
  }

  int counter() const {
    return counter_;
  }

  void open() {
    if (counter_) {
      file_ = LittleFS.open(path().c_str(), "r");
    }
  }

  void close() {
    if (file_) file_.close();
  }

  void rewind() {
    if (file_) file_.seek(8);	// Skip the header of the TIFF file.
  }

  void read(uint8_t *data, int size) {
    if (file_) {
      file_.read(data, size);
    } else {
      for (int i = 0; i < size; i++) data[i] = 0xff;	// The 0-th photo is a virtual photo filled in white.
    }
  }

  void next(int delta) {
    while (true) {
      counter_ = (counter_ + delta) % 100;
      while (counter_ < 0) counter_ += 100;
      if (counter_ == 0 || LittleFS.exists(path().c_str())) break;
    }
  }

private:
  String path() {
    String path = String(counter_);
    if (path.length() < 2) path = "0" + path;
    path = "/" + path + ".tif";
    return path;
  }

  int counter_;
  File file_;
};

// Draw a message to the screen (0: CONF, 1: TRAN, 2: DONE, 3: BATT).
void drawMessage(int msg) {
  uint8_t buf[WIDTH / 4];
  epd.enable();

  // Clear the screen.
  for (int i = 0; i < WIDTH / 4; i++) buf[i] = 0xaa;
  for (int iter = 0; iter < 4; iter++) {
    epd.startFrame();
    for (int line = 0; line < 16 * 4; line++) epd.writeRow(WIDTH / 4, buf);
    epd.endFrame();
  }

  epd.startFrame();
  for (int y = 0; y < 16; y++) {
    const uint8_t *ptr = font[y] + msg * 4;
    for (int x = 0; x < 32; x++) {
      buf[x] = ((ptr[x / 8] << (x % 8)) & 0x80) ? 0x55 : 0x00;
    }
    for (int line = 0; line < 4; line++) {
      epd.writeRow(WIDTH / 4, buf);
    }
  }
  epd.endFrame();
  epd.disable();
}

float getVoltage() {
  btStart();
  delay(1000);
  float vdd = 0.0;
  for (int i = 0; i < 100; i++) {
    delay(10);
    vdd += rom_phy_get_vdd33();
  }
  btStop();
  vdd /= 100.0;
  vdd = -0.0000135277 * vdd * vdd + 0.0128399 * vdd + 0.474502;
  return vdd;
}

void shutdown() {
  Serial.println("Battery voltage is low.");
  drawMessage(3);
  Serial.println("Shutting down.");
  esp_deep_sleep_start();  // Sleep indefinitely.
}

String url_decode(const String &str) {
  String result;
  for (int i = 0; i < str.length(); i++) {
    const char c = str[i];
    if (c == '+') {
      result.concat(" ");
    } else if (c == '%' && i + 2 < str.length()) {
      const char c0 = str[++i];
      const char c1 = str[++i];
      unsigned char d = 0;
      d += (c0 <= '9') ? c0 - '0' : (c0 <= 'F') ? c0 - 'A' + 10 : c0 - 'a' + 10;
      d <<= 4;
      d += (c1 <= '9') ? c1 - '0' : (c1 <= 'F') ? c1 - 'A' + 10 : c1 - 'a' + 10;
      result.concat((char)d);
    } else {
      result.concat(c);
    }
  }
  return result;
}

void callback() {
}

void config() {
  Serial.println("Entering the configuration mode.");
  drawMessage(0);
  preferences.begin("config", false);
  Serial.println("Free entries: " + String(preferences.freeEntries()));
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32", "12345678");
  delay(100);
  WiFi.softAPConfig(IPAddress(192, 168, 0, 1), IPAddress(192, 168, 0, 1), IPAddress(255, 255, 255, 0));

  WiFiServer server(80);
  server.begin();
  uint32_t last_connection = millis();
  while (true) {
    WiFiClient client = server.available();
    if (client) {
      const String line = client.readStringUntil('\n');
      Serial.println("Accessed: " + line);
      String message;
      if (line.startsWith("GET /?")) {
        String key;
        String val;
        String buf = line.substring(6);
        int pos = buf.indexOf(" ");
        if (pos < 0) pos = 0;
        buf = buf.substring(0, pos);
        buf.concat("&");
        while (buf.length()) {
          int pos = buf.indexOf("&");
          const String param = buf.substring(0, pos);
          buf = buf.substring(pos + 1);
          pos = param.indexOf("=");
          if (pos < 0) continue;
          if (param.substring(0, pos) == "key") key = url_decode(param.substring(pos + 1));
          else if (param.substring(0, pos) == "val") val = url_decode(param.substring(pos + 1));
        }
        key.trim();
        val.trim();
        Serial.println("key=" + key + ", val=" + val);
        if (key.length()) {
          preferences.putString(key.c_str(), val);
          if (preferences.getString(key.c_str()) == val) {
            message = "Succeeded to update: " + key;
          } else {
            message = "Failed to write: " + key;
          }
        } else {
          message = "Key was not found.";
        }
      }

      client.println("<!DOCTYPE html>");
      client.println("<head><title>Configuration</title></head>");
      client.println("<body>");
      client.println("<h1>Configuration</h1>");
      client.println("<form action=\"/\" method=\"get\">Key: <input type=\"text\" name=\"key\" size=\"10\"> Value: <input type=\"text\" name=\"val\" size=\"20\"> <input type=\"submit\"></form>");
      client.println("<p>" + message + "</p>");
      client.println("</body>");
      client.println("</html>");
      client.stop();
      last_connection = millis();
    }

    if (millis() - last_connection > 60000 || touchRead(T2) < TOUCH_THRESHOLD) break;
  }

  Serial.println("Disconnected.");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  preferences.end();
  drawMessage(2);
}

void transfer() {
  drawMessage(1);
  Serial.println("Entering the transfer mode.");
  preferences.begin("config", true);
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to " + preferences.getString("SSID"));
  WiFi.begin(preferences.getString("SSID").c_str(), preferences.getString("PASS").c_str());
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() >= 30000) return;
    Serial.print(".");
    delay(500);
  }
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());

  WiFiServer server(80);
  server.begin();
  uint32_t last_connection = millis();
  while (true) {
    WiFiClient client = server.available();
    if (client) {
      const String line = client.readStringUntil('\n');
      Serial.println("Accessed: " + line);
      String message;
      if (line.startsWith("GET /?")) {
        String submit;
        String file;
        String buf = line.substring(6);
        int pos = buf.indexOf(" ");
        if (pos < 0) pos = 0;
        buf = buf.substring(0, pos);
        buf.concat("&");
        while (buf.length()) {
          int pos = buf.indexOf("&");
          const String param = buf.substring(0, pos);
          buf = buf.substring(pos + 1);
          pos = param.indexOf("=");
          if (pos < 0) continue;
          if (param.substring(0, pos) == "submit") submit = url_decode(param.substring(pos + 1));
          else if (param.substring(0, pos) == "file") file = url_decode(param.substring(pos + 1));
        }
        Serial.println("submit=" + submit + ", file=" + file + ".");
        if (submit == "Format") {
          if (LittleFS.format()) {
            message = "Succeeded to format.";
          } else {
            message = "Failed to format.";
          }
        } else if (submit == "Remove") {
          if (LittleFS.remove("/" + file)) {
            message = "Succeeded to remove.";
          } else {
            message = "Failed to remove.";
          }
        }
      } else if (line.startsWith("POST / ")) {
        String boundary;
        String filename;
        while (client.connected() && client.available()) {
          String line = client.readStringUntil('\n');
          line.trim();
          if (line.startsWith("--")) {
            boundary = line;
            Serial.print("Boundary: "); Serial.println(boundary);
          } else if (boundary.length() && line.indexOf("name=\"file\"") >= 0) {
            const int bgn = line.indexOf("filename=\"");
            if (bgn >= 0) {
              const int end = line.indexOf("\"", bgn + 10);
              if (end >= 0) {
                filename = line.substring(bgn + 10, end);
              }
            }
            Serial.print("Filename: "); Serial.println(filename);
          } else if (filename.length() && line == "") {
            break;
          }
        }
        if (message.length() == 0 && filename.length() == 0) {
          message = "Filename is missing.";
        }
        if (message.length() == 0) {
          filename = "/" + filename;
          if (LittleFS.exists(filename)) LittleFS.remove(filename);
          File file = LittleFS.open(filename, FILE_WRITE);
          if (file) {
            Serial.print("Writing... ");
            boundary = "\r\n" + boundary + "--\r\n";
            static unsigned char *buf = new unsigned char[16 * 1024 + 512 + 1];
            size_t ofst = 0;
            while (client.connected() && client.available()) {
              const size_t avil = 16 * 1024 + 512 - ofst;
              size_t size = client.readBytes(buf + ofst, avil);
              ofst += size;
              if (ofst >= 16 * 1024 + boundary.length()) {
                file.write(buf, 16 * 1024);
                ofst -= 16 * 1024;
                memcpy(buf, buf + 16 * 1024, ofst);
              }
            }
            buf[ofst] = '\0';
            if (String((char *)(buf + ofst - boundary.length())).equals(boundary)) {
              file.write(buf, ofst - boundary.length());
              message = "Succeeded to upload.";
            }
            Serial.println("done");
            file.close();
          } else {
            message = "Failed to open.";
          }
        }
      }

      client.println("<!DOCTYPE html>");
      client.println("<head><title>Transfer</title></head>");
      client.println("<body>");
      client.println("<h1>Transfer</h1>");
      client.println("<form action=\"/\" method=\"post\" enctype=\"multipart/form-data\"><input type=\"submit\" name=\"submit\" value=\"Upload\"> <input type=\"file\" name=\"file\"></form>");
      client.println("<form action=\"/\" method=\"get\"><input type=\"submit\" name=\"submit\" value=\"Remove\"> <input type=\"text\" name=\"file\"></form>");
      client.println("<form action=\"/\" method=\"get\"><input type=\"submit\" name=\"submit\" value=\"Format\"></form>");
      client.println("<p>" + message + "</p>");
      client.println("<h1>Files</h1>");
      client.println("<pre>");
      File root = LittleFS.open("/");
      if (root) {
        for (File file = root.openNextFile(); file; file = root.openNextFile()) {
          client.print(file.size());
          client.print("\t");
          client.println(file.name());
          file.close();
        }
        root.close();
        const unsigned long used_size = LittleFS.usedBytes();
        client.print(used_size);
        client.println("\tUSED");
        const unsigned long free_size = LittleFS.totalBytes() - used_size;
        client.print(free_size);
        client.println("\tFREE");
      }
      client.println("</pre>");
      client.println("</body>");
      client.println("</html>");
      client.stop();
      Serial.println("Closed.");
      last_connection = millis();
    }

    if (millis() - last_connection > 60000 || touchRead(T0) < TOUCH_THRESHOLD) break;
  }

  Serial.println("Disconnected.");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  preferences.end();
  drawMessage(2);
}

void display(Photo *photo) {
  Serial.println("Drawing: " + String(photo->counter()));

  uint8_t img[WIDTH / 2];
  uint8_t buf[WIDTH / 4];
  epd.enable();

  // Clear the screen.
  for (int iter = 0; iter < 32; iter++) {
    if (iter < 24) {	// Black.
      for (int i = 0; i < WIDTH / 4; i++) buf[i] = 0x55;
    } else {	// White.
      for (int i = 0; i < WIDTH / 4; i++) buf[i] = 0xaa;
    }
    epd.startFrame();
    for (int line = 0; line < HEIGHT; line++) epd.writeRow(WIDTH / 4, buf);
    epd.endFrame();
  }

  photo->open();
  for (int grayscale = 14; grayscale >= 0; grayscale--) {
    // Output the image.
    photo->rewind();
    epd.startFrame();
    for (int line = 0; line < HEIGHT; line++) {
      photo->read(img, WIDTH / 2);

      uint8_t *ptr = img;
      for (int i = 0; i < WIDTH / 4; i++) {
        uint8_t b = 0;
        if ((*ptr >> 4)     <= grayscale) b |= 0x40;
        if ((*ptr++ & 0x0f) <= grayscale) b |= 0x10;
        if ((*ptr >> 4)     <= grayscale) b |= 0x04;
        if ((*ptr++ & 0x0f) <= grayscale) b |= 0x01;
        buf[i] = b;
      }

      epd.writeRow(WIDTH / 4, buf);
    }
    epd.endFrame();
  }
  epd.disable();
  photo->close();
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brown-out detection.
  Serial.begin(115200);
  while (!Serial) ;
  Serial.println("EPD-Photo");

  // Determine the mode.
  int mode = 0;
  while (touchRead(T0) < TOUCH_THRESHOLD) {
    mode = 3;
    if (millis() > 3000) {
      mode = 1;
      break;
    }
  }
  while (touchRead(T2) < TOUCH_THRESHOLD) {
    mode = 4;
    if (millis() > 3000) {
      mode = 2;
      break;
    }
  }
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TOUCHPAD) mode = 0;
  Serial.println("Mode=" + String(mode));

  // Initialize the filesystem.
  if (mode >= 2) {
    Serial.print("Mounting LittleFS... ");
    LittleFS.begin(true);
    Serial.println("done.");
  }

  // Process the mode.
  if (mode == 1) {
    config();
  } else if (mode == 2) {
    LittleFS.begin(true);
    transfer();
  } else if (mode == 3 || mode == 4) {
    Photo photo(photo_counter);
    photo.next((mode == 3) ? -1 : +1);
    photo_counter = photo.counter();
    display(&photo);
    LittleFS.end();
  }

  if (mode >= 2) {
    LittleFS.end();
  }

  // Check the battery voltage.
  const float voltage = getVoltage();
  Serial.println("Battery voltage: " + String(voltage));
  if (voltage < SHUTDOWN_VOLTAGE) shutdown();

  // Deep sleep.
  Serial.println("Sleeping.");
  touchAttachInterrupt(T0, callback, TOUCH_THRESHOLD);
  touchAttachInterrupt(T2, callback, TOUCH_THRESHOLD);
  esp_sleep_enable_touchpad_wakeup();
  esp_deep_sleep_start();
}

void loop() {
}
