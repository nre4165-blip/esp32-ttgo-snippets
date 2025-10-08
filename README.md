ESP32 - lilygo t-display esp32 Weather Station with TFT Display and OTA
This is a project for a compact weather station based on the ESP32 microcontroller and a TFT display (lilygo t-display esp32). The device connects to Wi-Fi to fetch accurate time and weather forecasts from the internet and supports Over-the-Air (OTA) firmware updates.

(Replace this link with a photograph of your assembled device)

Key Features
Current Time & Weather: Displays a large clock, current temperature, and a weather icon on the main screen.

5-Day Forecast: A button press switches the display to a detailed 5-day forecast (day/night temperature and weather icon).

Automatic Sync: Time is synchronized via an NTP server, and the weather is updated from the OpenWeatherMap API every 6 hours.

Smart Wi-Fi Reconnection: If the Wi-Fi connection is lost, the device attempts to reconnect. After the first failed attempt, it introduces increasing delays (5 min, 30 min, 1 hr) between subsequent attempts to avoid network spamming.

Over-the-Air (OTA) Updates: A built-in web server allows you to update the device's firmware through a web browser, without needing a physical connection to a computer.

Informative Display: The screen shows the current Wi-Fi connection status and a countdown timer for the next reconnection attempt.

Hardware Requirements
ESP32 Board with an integrated TFT display (e.g., LILYGO T-Display TTGO).

Button: Uses the onboard BOOT button (GPIO 0) or another button connected to GPIO 25.

Micro-USB/USB-C Cable for the initial flashing and power.

Software & PlatformIO Setup
This project is configured for the PlatformIO IDE.

Make sure you have Visual Studio Code with the PlatformIO IDE extension installed.

The required libraries will be downloaded automatically by PlatformIO based on the platformio.ini file.

Here is an example platformio.ini configuration. You should adapt the board setting to match your specific hardware.

Ini, TOML

 ; PlatformIO Project Configuration File
;
;   Build options: build flags, source filter
;   Upload options: custom upload port, speed and extra flags
;   Library options: dependencies, extra library storages
;   Advanced options: extra scripting
;
; Please visit documentation for the other options and examples
; https://docs.platformio.org/page/projectconf.html

[platformio]
; This specifies which environment will be run "by default"
; when you just click "Upload".
;
; !! SET THIS to 'lilygo-usb' for the VERY FIRST upload (to get the initial firmware onto the device).
; !! AFTER THAT, change this to 'lilygo-ota' for all future updates over Wi-Fi.
; default_envs = lilygo-usb

default_envs = lilygo-ota

[env]
; --- COMMON SETTINGS FOR BOTH ENVIRONMENTS ---
platform = espressif32@6.12.0
board = lilygo-t-display
framework = arduino
monitor_speed = 115200
board_build.partitions = partitions_ota.csv

lib_deps =
    bodmer/TFT_eSPI @ 2.5.43
    bblanchon/ArduinoJson @ 7.4.2
    paulstoffregen/OneWire
    milesburton/DallasTemperature
    links2004/WebSockets 
    tzapu/WiFiManager

; -----------------------------------------------
;   ENVIRONMENT 1: UPLOAD VIA USB (CABLE)
;   (Use this for the initial flash or for emergency recovery)
; -----------------------------------------------
[env:lilygo-usb]
; This environment inherits all settings from the [env] section
; We can specify the port manually OR comment out this line to let PIO find it automatically
; upload_port = COM3
; Standard upload protocol for USB; no authentication is needed.
upload_protocol = esptool

; -----------------------------------------------
;   ENVIRONMENT 2: UPLOAD VIA WI-FI (OTA / OVER-THE-AIR)
;   (Use this for all regular updates AFTER the initial flash)
; -----------------------------------------------
[env:lilygo-ota]
; This environment also inherits settings from the [env] section
; Explicitly tell PlatformIO to use the 'espota' protocol
upload_protocol = espota
upload_port = hive-monitor-tft 
upload_flags = --auth=admin_ota_pass

Configuration
Before compiling and uploading, you must edit the following lines in the src/main.cpp file:

Wi-Fi Credentials: Enter your network name (SSID) and password.

C++

const char* ssid = "Your_SSID";
const char* password = "Your_Password";
OpenWeatherMap API Key: Sign up on OpenWeatherMap to get your free API key and paste it here.

C++

String weatherApiKey = "YOUR_OPENWEATHERMAP_API_KEY";
Coordinates: Enter the latitude and longitude for your location. The values below are placeholders.

C++

// Coordinates for Your City, Your Region
const float lat = 12.34; // Latitude
const float lon = 56.78; // Longitude
Time Zone: Configure your time zone's offset from UTC in seconds and the daylight saving offset.

C++

const long  gmtOffset_sec = -18000;    // Example: UTC-5 (EST) -> -5 * 3600 = -18000
const int   daylightOffset_sec = 3600; // Standard Daylight Saving offset is 1 hour = 3600 seconds
Installation & Upload
Clone this repository or download the source code.

Open the project folder in Visual Studio Code.

Modify the configuration in src/main.cpp as described above.

Connect your ESP32 board to your computer.

PlatformIO should automatically detect the board and COM port.

Click the "Upload" button (right arrow icon) in the PlatformIO toolbar at the bottom of the VS Code window.

Usage
Main Screen: On startup, the device will attempt to connect to Wi-Fi. Once connected, it will display the current time, temperature, and a weather icon.

Forecast Screen: A short press of the BOOT button (GPIO 0 or GPIO 25) will switch the display to the 5-day forecast mode.

Return to Main Screen: The forecast screen will automatically time out and return to the main screen after 30 seconds, or you can press the button again to switch back immediately.

Over-the-Air (OTA) Firmware Updates
This feature allows for wireless firmware updates.

Find the IP Address: Once the device connects to Wi-Fi, its IP address will be printed to the Serial Monitor. You can open the monitor in PlatformIO (plug icon in the toolbar).

Compile the Binary: After making your code changes, build the project using the "Build" button (check mark icon) in the PlatformIO toolbar. The compiled firmware file will be located at .pio/build/<your_env_name>/firmware.bin.

Open the Web Interface: Enter the device's IP address into your web browser.

Upload Firmware: On the webpage, click "Choose File", select the firmware.bin file you just compiled, and click "Update Firmware".

Reboot: The device will automatically reboot with the new firmware after the update is complete.

Important: Do not disconnect power from the device during the OTA update process.
