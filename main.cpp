// This is the 0.2 version of the code, with improved WiFi reconnection logic.

// ---------- Required Libraries ----------
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "time.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Update.h>

// ---------- Wi-Fi and Time Settings ----------
const char* ssid = "D";
const char* password = "";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 7200;
const int   daylightOffset_sec = 3600;
unsigned long lastNtpSync = 0;
const unsigned long ntpSyncInterval = 6UL * 3600UL * 1000UL;

// ---------- Weather API Settings ----------
String weatherApiKey = "a8a283f17df3724e86fd204d747a398f"; // Your API key
// Coordinates for  
const float lat = 00.00;
const float lon = 00.00;
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 6UL * 3600UL * 1000UL;

// ---------- Web Server ----------
WebServer server(80);
bool ota_in_progress = false;

// ---------- Display and Button Settings ----------
TFT_eSPI tft = TFT_eSPI();
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define BUTTON_PIN 25 // The BOOT button on the board is connected to GPIO 0

// ---------- Global State Variables ----------
struct WeatherData { int temp = 0; String icon = "unknown"; };
WeatherData todayWeather;
String headerTime = "--:--";
bool needScreenRedraw = true;
bool wifiOK = false;
unsigned long lastScreenUpdate = 0; // Timer for screen updates

// Structure and array for 5-day forecast
struct DailyForecast {
  int temp_day = 0;
  int temp_night = 100; // Initial value for correct min search
  String icon = "unknown";
  int day_of_week = 0; // 0=SUN, 1=MON, ...
};
DailyForecast forecastData[5];
// <<< CHANGED: Days of the week are transliterated
const char* daysOfWeek[7] = {"НД", "PN", "VT", "SR", "CHT", "PT", "SB"};


// Variables for managing forecast mode
bool isForecastMode = false;
unsigned long forecastStartTime = 0;
unsigned long lastButtonPress = 0;
const unsigned long debounceTime = 500; // Debounce delay for the button

// ---------- WiFi Reconnection Logic ----------
unsigned long wifiConnectStart = 0;
unsigned long lastWifiAttempt = 0;
int wifiReconnectAttempts = 0;
bool isConnectingWifi = false;

// Weather icons (byte arrays)
const unsigned char sun_icon[] PROGMEM = {
  0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x42, 0x00, 0x0e,
  0x00, 0x00, 0x21, 0x08, 0x84, 0x20, 0x10, 0x80, 0x00, 0x7f, 0xfc, 0x00, 0x01, 0x80, 0x10, 0x80,
  0x84, 0x20, 0x00, 0x42, 0x00, 0x0e, 0x00, 0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80,
  0x00, 0x00
};
const unsigned char cloud_icon[] PROGMEM = {
  0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x00, 0x00, 0x08, 0x1f, 0xf8,
  0x00, 0x00, 0x00, 0x1f, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xe0, 0x00, 0x00, 0x01, 0xff,
  0xff, 0xf0, 0x00, 0x00, 0x03, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x03, 0xff, 0xff, 0xf8, 0x00, 0x00,
  0x01, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x1f, 0xff, 0x80,
  0x00, 0x00, 0x00, 0x0f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char rain_icon[] PROGMEM = {
  0x00, 0xe0, 0x07, 0xf8, 0x1f, 0xfc, 0x7f, 0xfe, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xfe, 0x1f, 0xfc,
  0x0f, 0xf8, 0x03, 0xe0, 0x00, 0x00, 0x04, 0x20, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x12, 0x40,
  0x00, 0x00, 0x04, 0x20, 0x00, 0x00
};
const unsigned char unknown_icon[] PROGMEM = {
  0x00, 0x00, 0x01, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x70, 0x00, 0x00, 0x00, 0x38, 0x1c, 0x00,
  0x00, 0x00, 0x60, 0x06, 0x00, 0x00, 0x00, 0xc0, 0x03, 0x00, 0x00, 0x00, 0x80, 0x01, 0x80, 0x00,
  0x01, 0x00, 0x01, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  0x00, 0x00, 0x02, 0x00, 0x00
};


// ---------- OTA HTML ----------
String generateUpdatePage() {
  return R"rawliteral(
<!DOCTYPE html><html><head><title>ESP32 Firmware Update</title>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<style>body{font-family:sans-serif;background-color:#f0f0f0;margin:2em;}
.container{max-width:400px;margin:auto;padding:1em;border:1px solid #ccc;border-radius:8px;background-color:#fff;}
h1{text-align:center;color:#333;}
form{display:flex;flex-direction:column;gap:1em;}
input[type=file]{padding:10px;border:1px solid #ccc;border-radius:4px;}
input[type=submit]{background-color:#007bff;color:white;padding:12px;border:none;border-radius:4px;cursor:pointer;font-size:1em;}
input[type=submit]:hover{background-color:#0056b3;}
</style></head><body><div class='container'><h1>Firmware Update</h1>
<form method='POST' action='/update' enctype='multipart/form-data'>
<input type='file' name='update' accept='.bin' required>
<input type='submit' value='Update Firmware'>
</form></div></body></html>)rawliteral";
}

// ---------- WebServer Handlers ----------
void handleRoot() { server.send(200, "text/html", generateUpdatePage()); }
void handleUpdate() { server.send(200, "text/html", "<p>Update Success! Rebooting in 3 seconds...</p><meta http-equiv='refresh' content='3;url=/' />"); delay(100); ESP.restart(); }
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    ota_in_progress = true;
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { Update.printError(Serial); }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) { Update.printError(Serial); }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { Serial.println("Update successful."); }
    else { Update.printError(Serial); }
    ota_in_progress = false;
  }
}

// ---------- Weather ----------
String mapIcon(String iconCode) {
  if (iconCode.startsWith("01")) return "sun";
  if (iconCode.startsWith("02") || iconCode.startsWith("03") || iconCode.startsWith("04")) return "cloud";
  if (iconCode.startsWith("09") || iconCode.startsWith("10") || iconCode.startsWith("11")) return "rain";
  return "unknown";
}

void updateWeatherData() {
  if (!wifiOK || weatherApiKey.length() < 10) return;

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/forecast?lat=" + String(lat) + "&lon=" + String(lon) + "&appid=" + weatherApiKey + "&units=metric&cnt=40";
  http.begin(url);
  http.setTimeout(10000);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(8192);
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      todayWeather.temp = round(doc["list"][0]["main"]["temp"].as<float>());
      todayWeather.icon = mapIcon(doc["list"][0]["weather"][0]["icon"].as<String>());

      for (int i = 0; i < 5; ++i) {
        forecastData[i] = DailyForecast();
      }

      int currentDayIndex = -1;
      int lastDayOfYear = -1;

      JsonArray list = doc["list"].as<JsonArray>();
      for (JsonObject item : list) {
        long timestamp = item["dt"];
        struct tm timeinfo;
        gmtime_r(&timestamp, &timeinfo);
        int dayOfYear = timeinfo.tm_yday;

        if (dayOfYear != lastDayOfYear) {
          lastDayOfYear = dayOfYear;
          currentDayIndex++;
          if (currentDayIndex >= 5) break;

          forecastData[currentDayIndex].day_of_week = timeinfo.tm_wday;
        }

        float temp = item["main"]["temp"];
        if (temp > forecastData[currentDayIndex].temp_day) {
          forecastData[currentDayIndex].temp_day = round(temp);
        }
        if (temp < forecastData[currentDayIndex].temp_night) {
          forecastData[currentDayIndex].temp_night = round(temp);
        }

        if (timeinfo.tm_hour >= 12 && timeinfo.tm_hour <= 15) {
          forecastData[currentDayIndex].icon = mapIcon(item["weather"][0]["icon"].as<String>());
        } else if (forecastData[currentDayIndex].icon == "unknown") {
           forecastData[currentDayIndex].icon = mapIcon(item["weather"][0]["icon"].as<String>());
        }
      }

      needScreenRedraw = true;
      Serial.println("Weather data updated for 5 days.");

    } else {
      Serial.print("JSON parse error: ");
      Serial.println(err.c_str());
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  lastWeatherUpdate = millis();
}

// ---------- Time ----------
void updateTimeStrings() {
  if (!wifiOK) return;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeBuf[6];
    strftime(timeBuf, 6, "%H:%M", &timeinfo);
    if (String(timeBuf) != headerTime) {
      headerTime = String(timeBuf);
      needScreenRedraw = true;
    }
  }
}

void syncNTP() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  lastNtpSync = millis();
}

// ---------- Display ----------
void drawWeatherIcon(String icon, int x, int y, int w, int h) {
  if (icon == "sun") tft.drawXBitmap(x, y, sun_icon, w, h, TFT_YELLOW);
  else if (icon == "cloud") tft.drawXBitmap(x, y, cloud_icon, w, h, TFT_WHITE);
  else if (icon == "rain") tft.drawXBitmap(x, y, rain_icon, w, h, TFT_CYAN);
  else tft.drawXBitmap(x, y, unknown_icon, w, h, TFT_RED);
}

void drawMainScreen() {
  tft.fillScreen(TFT_BLACK);

  if (wifiOK) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(8);
    tft.setTextSize(1);
    tft.drawString(headerTime, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 25);

    String tempStr = String(todayWeather.temp) + (char)247 + "C";
    tft.setTextFont(4);
    tft.setTextDatum(TC_DATUM);
    int iconWidth = 36;
    int iconHeight = 36;
    int textWidth = tft.textWidth(tempStr);
    int totalWeatherWidth = iconWidth + textWidth + 5;
    int weatherY = SCREEN_HEIGHT - 30;
    int iconX = (SCREEN_WIDTH - totalWeatherWidth) / 2;
    drawWeatherIcon(todayWeather.icon, iconX, weatherY - (iconHeight / 2), iconWidth, iconHeight);
    int textX = iconX + iconWidth + 5;
    tft.drawString(tempStr, textX, weatherY);

  } else {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);

    if (isConnectingWifi) {
      tft.drawString("Connecting to WiFi...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    } else if (wifiReconnectAttempts > 0) {
      unsigned long waitDurationSec = 0;
      switch (wifiReconnectAttempts) {
        case 1:  waitDurationSec = 5UL * 60UL; break;
        case 2:  waitDurationSec = 30UL * 60UL; break;
        default: waitDurationSec = 60UL * 60UL; break;
      }
      unsigned long elapsedSec = (millis() - lastWifiAttempt) / 1000;
      unsigned long remainingSec = (elapsedSec < waitDurationSec) ? (waitDurationSec - elapsedSec) : 0;

      String timeStr;
      if (remainingSec >= 120) {
        timeStr = String((remainingSec + 59) / 60) + " min"; // Round up to nearest minute
      } else {
        timeStr = String(remainingSec) + " sec";
      }

      tft.drawString("WiFi disconnected.", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10);
      tft.drawString("Next attempt in " + timeStr, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 10);
    } else {
      tft.drawString("Connecting to WiFi...", SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2);
    }
  }
  needScreenRedraw = false; // The loop will decide when to redraw again
}

void drawForecastScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(TC_DATUM);

  int colWidth = SCREEN_WIDTH / 5; // 240 / 5 = 48px

  for (int i = 0; i < 5; i++) {
    int colX = i * colWidth;

    // --- Draw day of the week ---
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextFont(2);
    tft.drawString(daysOfWeek[forecastData[i].day_of_week], colX + colWidth / 2, 5);

    // --- Draw icon (Y changed) ---
    int iconSize = 32;
    drawWeatherIcon(forecastData[i].icon, colX + (colWidth - iconSize) / 2, 25, iconSize, iconSize);

    // --- Draw temperature (font increased and Y changed) ---
    tft.setTextFont(4); // <<< CHANGED: Font increased
    
    // Daytime temperature
    String dayTempStr = String(forecastData[i].temp_day) + (char)247;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(dayTempStr, colX + colWidth / 2, 70);

    // Nighttime temperature
    String nightTempStr = String(forecastData[i].temp_night) + (char)247;
    tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    tft.drawString(nightTempStr, colX + colWidth / 2, 105);

    // --- Draw vertical separator ---
    if (i < 4) { // <<< NEW: Separator added
      tft.drawFastVLine(colX + colWidth -1, 5, SCREEN_HEIGHT - 10, TFT_DARKGREY);
    }
  }
}


// ---------- WiFi ----------
void startWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  wifiConnectStart = millis();
  isConnectingWifi = true;
  Serial.println("Starting WiFi connection attempt (20s timeout)...");
  needScreenRedraw = true;
}

void checkWiFi() {
  // Case 1: We are connected.
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiOK) {
      wifiOK = true;
      isConnectingWifi = false;
      wifiReconnectAttempts = 0;
      Serial.println("WiFi connected!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      server.on("/", HTTP_GET, handleRoot);
      server.on("/update", HTTP_POST, handleUpdate, handleFileUpload);
      server.begin();
      syncNTP();
      updateTimeStrings(); // <<< NEW: Forcing time update immediately after synchronization
      updateWeatherData();
    }
    return;
  }

  // Case 2: We are not connected.
  if (wifiOK) {
    // We just lost connection
    wifiOK = false;
    isConnectingWifi = false;
    wifiReconnectAttempts = 0;
    Serial.println("WiFi connection lost. Starting reconnect procedure.");
    startWiFi(); // Start the first 20-second attempt immediately
    return;
  }

  if (isConnectingWifi) {
    // We are actively trying to connect
    if (millis() - wifiConnectStart >= 20000) {
      // The 20s attempt failed
      Serial.println("Connection attempt timed out.");
      isConnectingWifi = false;
      wifiReconnectAttempts++;
      lastWifiAttempt = millis();
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      needScreenRedraw = true;
    }
  } else {
    // We are in a wait period
    if (wifiReconnectAttempts > 0) {
      unsigned long waitDuration = 0;
      switch (wifiReconnectAttempts) {
        case 1:  waitDuration = 5UL * 60UL * 1000UL; break;   // 5 minutes
        case 2:  waitDuration = 30UL * 60UL * 1000UL; break;  // 30 minutes
        default: waitDuration = 60UL * 60UL * 1000UL; break;  // 1 hour
      }
      if (millis() - lastWifiAttempt >= waitDuration) {
        startWiFi();
      }
    }
  }
}

// ---------- Setup & Loop ----------
void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  startWiFi();
}

void loop() {
  if (wifiOK) {
    server.handleClient();
  }

  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastButtonPress > debounceTime) {
    lastButtonPress = millis();
    if (!ota_in_progress) {
      isForecastMode = !isForecastMode;
      if (isForecastMode) {
        forecastStartTime = millis();
        drawForecastScreen();
      } else {
        needScreenRedraw = true;
      }
    }
  }

  if (!ota_in_progress) {
    if (isForecastMode && (millis() - forecastStartTime > 30000)) { // 30 seconds
      isForecastMode = false;
      needScreenRedraw = true;
    }

    if (!isForecastMode) {
      checkWiFi();

      if (wifiOK) {
        updateTimeStrings();

        if ((millis() - lastWeatherUpdate > weatherUpdateInterval) || lastWeatherUpdate == 0) {
          updateWeatherData();
        }
        if (millis() - lastNtpSync > ntpSyncInterval) {
          syncNTP();
        }
      }

      if (needScreenRedraw) {
        drawMainScreen();
        lastScreenUpdate = millis();
      } else if (!wifiOK && (millis() - lastScreenUpdate > 1000)) {
        // When WiFi is down, update screen every second for the countdown
        drawMainScreen();
        lastScreenUpdate = millis();
      }
    }
  }

  delay(10);
}
