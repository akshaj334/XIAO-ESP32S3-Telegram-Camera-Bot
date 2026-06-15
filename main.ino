/*
  XIAO ESP32S3 Sense Telegram Camera Bot
  --------------------------------------------------
  Features:
  - Telegram-controlled photo capture
  - SD card photo saving
  - PIR motion-triggered capture
  - Hourly auto-send
  - Daily scheduled capture
  - Timed fun-photo triggers
  - Status, uptime, temperature, IP, MAC
  - Fun commands through /rick

  Board:
  - Seeed Studio XIAO ESP32S3 Sense

  Required Arduino IDE settings:
  - Board: Seeed XIAO ESP32S3
  - USB CDC On Boot: Enabled
  - PSRAM: OPI PSRAM
  - Partition Scheme: Huge APP / Maximum APP
  - Upload Speed: 115200

  Required libraries:
  - UniversalTelegramBot by Brian Lough
  - ArduinoJson by Benoit Blanchon
  - NTPClient by Fabrice Weinberg
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

#include "esp_camera.h"
#include "FS.h"
#include <SD.h>
#include <SPI.h>

#include <EEPROM.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include "secrets.h"
// ===================== USER CONFIGURATION =====================

const char* DEFAULT_SSID     = "YOUR_WIFI_NAME";
const char* DEFAULT_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN";
const char* CHAT_ID   = "YOUR_TELEGRAM_CHAT_ID";

// India time zone: UTC + 5:30
const long UTC_OFFSET_SECONDS = 19800;

// ===================== XIAO ESP32S3 SENSE CAMERA PINS =====================

#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1

#define XCLK_GPIO_NUM   10
#define SIOD_GPIO_NUM   40
#define SIOC_GPIO_NUM   39

#define Y9_GPIO_NUM     48
#define Y8_GPIO_NUM     11
#define Y7_GPIO_NUM     12
#define Y6_GPIO_NUM     14
#define Y5_GPIO_NUM     16
#define Y4_GPIO_NUM     18
#define Y3_GPIO_NUM     17
#define Y2_GPIO_NUM     15

#define VSYNC_GPIO_NUM  38
#define HREF_GPIO_NUM   47
#define PCLK_GPIO_NUM   13

// ===================== HARDWARE CONFIGURATION =====================

// XIAO ESP32S3 Sense does not have AI Thinker-style flash LED.
// Set this true only if you connect an external LED to FLASH_LED pin.
#define USE_FLASH_LED false
#define FLASH_LED     4

// PIR OUT pin: connect PIR OUT to XIAO D0 / GPIO1
#define PIR_SENSOR    1

// XIAO ESP32S3 Sense SD card CS pin
#define SD_CS_PIN     21

// ===================== EEPROM SETTINGS =====================

#define EEPROM_SIZE 64

#define EEPROM_MOTION_ENABLED      0
#define EEPROM_AUTO_SEND_ENABLED   1
#define EEPROM_TIME_ENABLED        2
#define EEPROM_SCHEDULED_HOUR      3
#define EEPROM_SCHEDULED_MINUTE    4
#define EEPROM_TIMED_EGGS_ENABLED  5

// ===================== OBJECTS =====================

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET_SECONDS);

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ===================== GLOBAL STATES =====================

bool motionDetectionEnabled = false;
bool autoSendEnabled = false;
bool timeCaptureEnabled = false;
bool timedEasterEggsEnabled = true;

int scheduledHour = 7;
int scheduledMinute = 30;

unsigned long lastTelegramCheck = 0;
unsigned long lastAutoSendTime = 0;
unsigned long lastMotionTime = 0;
unsigned long lastCaptureTime = 0;
unsigned long uptimeStart = 0;

const unsigned long TELEGRAM_CHECK_INTERVAL = 1200;
const unsigned long AUTO_SEND_INTERVAL = 3600000UL;
const unsigned long MOTION_DEBOUNCE_TIME = 10000UL;

bool sdAvailable = false;
bool cameraReady = false;

camera_fb_t* telegram_fb = nullptr;
size_t telegram_fb_index = 0;

// ===================== TIMED FUN-PHOTO EVENTS =====================

struct TimedEgg {
  int hour;
  int minute;
  const char* message;
  long lastDay;
};

TimedEgg timedEggs[] = {
  {0, 0, "Midnight detected. Ghost camera activated. 👻", -1},
  {1, 11, "01:11 detected. The camera is judging your sleep schedule.", -1},
  {2, 22, "02:22 detected. Suspicious hour. Photo time.", -1},
  {3, 0, "It is 3 AM. The XIAO saw something... maybe you. 👀", -1},
  {3, 33, "03:33 detected. Triple trouble photo.", -1},
  {4, 4, "04:04 detected. Sleep not found.", -1},
  {5, 5, "05:05 detected. Early bird surveillance mode.", -1},
  {6, 9, "06:09 detected. Morning leak activated.", -1},
  {6, 30, "06:30 detected. Wake-up photo.", -1},
  {7, 7, "07:07 detected. Lucky photo.", -1},
  {8, 8, "08:08 detected. Symmetry photo.", -1},
  {9, 9, "09:09 detected. Camera says hello.", -1},
  {9, 45, "09:45 detected. Productivity check failed.", -1},
  {10, 10, "10:10 detected. Perfect clock photo.", -1},
  {11, 11, "11:11 detected. Make a wish. Photo taken.", -1},
  {12, 12, "12:12 detected. Lunch-time evidence captured.", -1},
  {12, 34, "12:34 detected. Sequential time photo.", -1},
  {13, 37, "13:37 detected. Leet photo activated. 😎", -1},
  {14, 14, "14:14 detected. Afternoon spy mode.", -1},
  {15, 15, "15:15 detected. Random photo attack.", -1},
  {15, 51, "15:51 detected. Mirror-time photo.", -1},
  {16, 16, "16:16 detected. Camera remembered you exist.", -1},
  {17, 17, "17:17 detected. Evening surveillance.", -1},
  {18, 18, "18:18 detected. Golden hour photo.", -1},
  {19, 19, "19:19 detected. Dinner-time camera leak.", -1},
  {20, 20, "20:20 detected. Vision mode activated.", -1},
  {21, 21, "21:21 detected. Night mode joke photo.", -1},
  {22, 22, "22:22 detected. Sleep warning photo.", -1},
  {23, 23, "23:23 detected. Last warning before midnight.", -1},
  {23, 59, "23:59 detected. Final photo before tomorrow.", -1}
};

const int timedEggCount = sizeof(timedEggs) / sizeof(timedEggs[0]);

// ===================== TELEGRAM PHOTO CALLBACKS =====================

bool isMoreDataAvailable() {
  return telegram_fb && telegram_fb_index < telegram_fb->len;
}

uint8_t getNextByte() {
  if (!telegram_fb || telegram_fb_index >= telegram_fb->len) return 0;
  return telegram_fb->buf[telegram_fb_index++];
}

// ===================== SETUP =====================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("XIAO ESP32S3 Sense Telegram Camera Bot Starting...");

  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  if (USE_FLASH_LED) {
    pinMode(FLASH_LED, OUTPUT);
    digitalWrite(FLASH_LED, LOW);
  }

  pinMode(PIR_SENSOR, INPUT);

  connectToWiFi();

  client.setInsecure();

  timeClient.begin();
  timeClient.update();

  cameraReady = initCamera();
  initSDCard();

  uptimeStart = millis();
  randomSeed(micros());

  sendStartupMessage();
}

// ===================== LOOP =====================

void loop() {
  maintainWiFi();
  updateTime();
  checkTelegramMessages();
  handleAutoSend();
  handleMotionDetection();
  checkScheduledCapture();
  checkTimedEasterEggs();
}

// ===================== SETTINGS =====================

void loadSettings() {
  motionDetectionEnabled = EEPROM.read(EEPROM_MOTION_ENABLED) == 1;
  autoSendEnabled = EEPROM.read(EEPROM_AUTO_SEND_ENABLED) == 1;
  timeCaptureEnabled = EEPROM.read(EEPROM_TIME_ENABLED) == 1;

  int eggValue = EEPROM.read(EEPROM_TIMED_EGGS_ENABLED);
  timedEasterEggsEnabled = (eggValue == 255 || eggValue == 1);

  int h = EEPROM.read(EEPROM_SCHEDULED_HOUR);
  int m = EEPROM.read(EEPROM_SCHEDULED_MINUTE);

  if (h >= 0 && h <= 23) scheduledHour = h;
  if (m >= 0 && m <= 59) scheduledMinute = m;
}

void saveSetting(int address, int value) {
  EEPROM.write(address, value);
  EEPROM.commit();
}

// ===================== WIFI =====================

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(DEFAULT_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connection failed. Check SSID/password.");
  }
}

void maintainWiFi() {
  static unsigned long lastReconnectAttempt = 0;

  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - lastReconnectAttempt >= 10000UL) {
    lastReconnectAttempt = millis();
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD);
  }
}

// ===================== TIME =====================

void updateTime() {
  static unsigned long lastNTPUpdate = 0;

  if (millis() - lastNTPUpdate >= 60000UL) {
    timeClient.update();
    lastNTPUpdate = millis();
  }
}

// ===================== CAMERA =====================

bool initCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 18;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  delay(500);

  sensor_t* s = esp_camera_sensor_get();

  if (s) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  }

  Serial.println("Camera initialized.");
  return true;
}

// ===================== SD CARD =====================

void initSDCard() {
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card not available. Continuing without SD.");
    sdAvailable = false;
    return;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No SD card detected.");
    sdAvailable = false;
    return;
  }

  sdAvailable = true;
  Serial.println("SD card initialized.");
}

// ===================== TELEGRAM =====================

void sendStartupMessage() {
  String msg = "XIAO ESP32S3 Sense Camera Bot is online.\n\n";
  msg += "Main commands:\n";
  msg += "/photo - Capture photo\n";
  msg += "/status - System status\n";
  msg += "/help - Command list\n";
  msg += "/rick - Fun commands\n\n";
  msg += "System:\n";
  msg += "Camera: ";
  msg += cameraReady ? "Ready\n" : "Not ready\n";
  msg += "SD card: ";
  msg += sdAvailable ? "Available\n" : "Not available\n";
  msg += "Timed photo events: ";
  msg += timedEasterEggsEnabled ? "Enabled\n" : "Disabled\n";

  bot.sendMessage(CHAT_ID, msg, "");
}

void checkTelegramMessages() {
  if (millis() - lastTelegramCheck < TELEGRAM_CHECK_INTERVAL) return;

  lastTelegramCheck = millis();

  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;

      if (chat_id != CHAT_ID) {
        bot.sendMessage(chat_id, "Unauthorized user.", "");
        continue;
      }

      handleCommand(text);
    }

    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

// ===================== COMMAND HANDLER =====================

void handleCommand(String commandOriginal) {
  commandOriginal.trim();

  String command = commandOriginal;
  command.toLowerCase();

  if (command == "/photo") {
    captureAndSendPhoto(true);
  }

  else if (command.startsWith("/stream")) {
    int numPhotos = parseNumberAfterCommand(command, 5);
    if (numPhotos < 1) numPhotos = 1;
    if (numPhotos > 20) numPhotos = 20;
    streamPhotos(numPhotos);
  }

  else if (command == "/motion on") {
    motionDetectionEnabled = true;
    saveSetting(EEPROM_MOTION_ENABLED, 1);
    bot.sendMessage(CHAT_ID, "Motion detection enabled.", "");
  }

  else if (command == "/motion off") {
    motionDetectionEnabled = false;
    saveSetting(EEPROM_MOTION_ENABLED, 0);
    bot.sendMessage(CHAT_ID, "Motion detection disabled.", "");
  }

  else if (command == "/autosend on") {
    autoSendEnabled = true;
    saveSetting(EEPROM_AUTO_SEND_ENABLED, 1);
    bot.sendMessage(CHAT_ID, "Auto-send enabled. One photo will be sent every hour.", "");
  }

  else if (command == "/autosend off") {
    autoSendEnabled = false;
    saveSetting(EEPROM_AUTO_SEND_ENABLED, 0);
    bot.sendMessage(CHAT_ID, "Auto-send disabled.", "");
  }

  else if (command.startsWith("/settime")) {
    handleSetTime(command);
  }

  else if (command == "/timecapture on") {
    timeCaptureEnabled = true;
    saveSetting(EEPROM_TIME_ENABLED, 1);
    bot.sendMessage(CHAT_ID, "Daily scheduled capture enabled.", "");
  }

  else if (command == "/timecapture off") {
    timeCaptureEnabled = false;
    saveSetting(EEPROM_TIME_ENABLED, 0);
    bot.sendMessage(CHAT_ID, "Daily scheduled capture disabled.", "");
  }

  else if (command == "/timeeggs on") {
    timedEasterEggsEnabled = true;
    saveSetting(EEPROM_TIMED_EGGS_ENABLED, 1);
    bot.sendMessage(CHAT_ID, "Timed photo events enabled.", "");
  }

  else if (command == "/timeeggs off") {
    timedEasterEggsEnabled = false;
    saveSetting(EEPROM_TIMED_EGGS_ENABLED, 0);
    bot.sendMessage(CHAT_ID, "Timed photo events disabled.", "");
  }

  else if (command == "/list") {
    listFiles();
  }

  else if (command.startsWith("/delete")) {
    String filename = commandOriginal.substring(7);
    filename.trim();
    deleteFile(filename);
  }

  else if (command == "/clear") {
    clearFiles();
  }

  else if (command == "/status") {
    sendStatus();
  }

  else if (command == "/uptime") {
    sendUptime();
  }

  else if (command == "/temp") {
    sendTemperature();
  }

  else if (command == "/ip") {
    bot.sendMessage(CHAT_ID, WiFi.localIP().toString(), "");
  }

  else if (command == "/mac") {
    bot.sendMessage(CHAT_ID, WiFi.macAddress(), "");
  }

  else if (command == "/reset") {
    resetSettings();
  }

  else if (command == "/help") {
    sendHelp();
  }

  else if (command == "/rick") {
    sendFunList();
  }

  else if (command == "/rickroll" || command == "/paraaksu" || command == "/paraxu") {
    bot.sendMessage(CHAT_ID, "Never gonna give you up, never gonna let you down... 🎶", "");
  }

  else if (command == "/whoami") {
    bot.sendMessage(CHAT_ID, "You are the reason mute buttons were invented.", "");
  }

  else if (command == "/selfdestruct") {
    bot.sendMessage(CHAT_ID, "Self-destruct sequence initiated.", "");
    delay(700);
    bot.sendMessage(CHAT_ID, "3...", "");
    delay(700);
    bot.sendMessage(CHAT_ID, "2...", "");
    delay(700);
    bot.sendMessage(CHAT_ID, "1...", "");
    delay(700);
    bot.sendMessage(CHAT_ID, "Sequence cancelled. Device integrity preserved.", "");
  }

  else if (command == "/hacker") {
    bot.sendMessage(CHAT_ID, "Running diagnostic simulation... 99%", "");
    delay(1200);
    bot.sendMessage(CHAT_ID, "Access denied.", "");
  }

  else if (command.startsWith("/ask")) {
    int r = random(0, 5);
    if (r == 0) bot.sendMessage(CHAT_ID, "Yes.", "");
    else if (r == 1) bot.sendMessage(CHAT_ID, "No.", "");
    else if (r == 2) bot.sendMessage(CHAT_ID, "Maybe.", "");
    else if (r == 3) bot.sendMessage(CHAT_ID, "Ask again after charging my battery.", "");
    else bot.sendMessage(CHAT_ID, "42.", "");
  }

  else if (command == "/coffee") {
    bot.sendMessage(CHAT_ID, "☕ Virtual coffee generated.", "");
  }

  else if (command == "/weather") {
    bot.sendMessage(CHAT_ID, "Weather status: always sunny in cyberspace.", "");
  }

  else if (command == "/secretcode") {
    bot.sendMessage(CHAT_ID, "Hidden feature unlocked.", "");
  }

  else if (command == "/helpme") {
    if (timeClient.getHours() == 3) {
      bot.sendMessage(CHAT_ID, "It is 3 AM. Sleep is strongly recommended.", "");
    } else {
      bot.sendMessage(CHAT_ID, "Assistance request received.", "");
    }
  }

  else if (command == "/hiddenfeature") {
    bot.sendMessage(CHAT_ID, "This feature is classified.", "");
  }

  else if (command == "/warpdrive" || command == "/wrapdrive") {
    bot.sendMessage(CHAT_ID, "Warp drive unavailable. Power budget insufficient.", "");
  }

  else if (command == "/hackme") {
    bot.sendMessage(CHAT_ID, "Security scan complete. User remains suspicious.", "");
  }

  else if (command == "/password123") {
    bot.sendMessage(CHAT_ID, "Weak password detected. Try harder.", "");
  }

  else if (command == "/scream") {
    bot.sendMessage(CHAT_ID, "AAAAAAAAAAAAAAAAAAAAAHHHHHHHHHHHHHHH!", "");
  }

  else if (command == "/snapme") {
    bot.sendMessage(CHAT_ID, "Visual scan requested. Use /photo for capture.", "");
  }

  else if (command == "/shutdown") {
    bot.sendMessage(CHAT_ID, "Shutdown request ignored. System remains online.", "");
  }

  else if (command == "/boostwifi") {
    bot.sendMessage(CHAT_ID, "WiFi boost simulated. Actual physics unchanged.", "");
  }

  else if (command == "/paranormal") {
    bot.sendMessage(CHAT_ID, motionDetectionEnabled ? "Motion scan active. No ghosts confirmed." : "Motion scan inactive. Ghost status unknown.", "");
  }

  else if (command == "/secretadmin") {
    bot.sendMessage(CHAT_ID, "Admin mode unavailable.", "");
  }

  else if (command == "/esp32") {
    bot.sendMessage(CHAT_ID, "ESP32 system online.", "");
  }

  else if (command == "/fakeupdate" || command == "/update") {
    bot.sendMessage(CHAT_ID, "Update simulation complete. No changes applied.", "");
  }

  else if (command == "/darkmode") {
    bot.sendMessage(CHAT_ID, "Dark mode acknowledged.", "");
  }

  else if (command == "/battery100") {
    bot.sendMessage(CHAT_ID, "Battery status simulation: 100%.", "");
  }

  else if (command == "/cheatcode") {
    bot.sendMessage(CHAT_ID, "Cheat code accepted. Reward: nothing.", "");
  }

  else if (command == "/coinflip") {
    bot.sendMessage(CHAT_ID, random(0, 2) == 0 ? "Heads" : "Tails", "");
  }

  else if (command == "/roll") {
    bot.sendMessage(CHAT_ID, "Dice result: " + String(random(1, 7)), "");
  }

  else if (command.startsWith("/rockpaperscissors")) {
    playRockPaperScissors(command);
  }

  else if (command == "/roastme") {
    bot.sendMessage(CHAT_ID, "You are not slow. You are just buffering in real life.", "");
  }

  else if (command == "/braincheck") {
    bot.sendMessage(CHAT_ID, "Brain scan complete: one tab open, 47 tabs frozen.", "");
  }

  else if (command == "/genius") {
    bot.sendMessage(CHAT_ID, "Genius mode requested. Hardware support not detected.", "");
  }

  else if (command == "/motivation") {
    bot.sendMessage(CHAT_ID, "You can do anything, except remember the correct command.", "");
  }

  else if (command == "/study") {
    bot.sendMessage(CHAT_ID, "Study mode unavailable. Motivation module missing.", "");
  }

  else if (command == "/focus") {
    bot.sendMessage(CHAT_ID, "Focus detected for 0.3 seconds. New record.", "");
  }

  else if (command == "/iq") {
    bot.sendMessage(CHAT_ID, "Calculating IQ. Please insert more RAM.", "");
  }

  else if (command == "/smart") {
    bot.sendMessage(CHAT_ID, "Smartness detected. Confidence level: experimental.", "");
  }

  else if (command == "/lazy") {
    bot.sendMessage(CHAT_ID, "Power-saving behavior detected.", "");
  }

  else if (command == "/legend") {
    bot.sendMessage(CHAT_ID, "Legend status pending verification.", "");
  }

  else {
    bot.sendMessage(CHAT_ID, "Unknown command. Use /help.", "");
  }
}

// ===================== COMMAND HELPERS =====================

int parseNumberAfterCommand(String command, int defaultValue) {
  int spaceIndex = command.indexOf(' ');
  if (spaceIndex == -1) return defaultValue;

  int value = command.substring(spaceIndex + 1).toInt();
  if (value <= 0) return defaultValue;

  return value;
}

// ===================== AUTO FEATURES =====================

void handleAutoSend() {
  if (!autoSendEnabled) return;

  if (millis() - lastAutoSendTime >= AUTO_SEND_INTERVAL) {
    captureAndSendPhoto(true);
    lastAutoSendTime = millis();
  }
}

void handleMotionDetection() {
  if (!motionDetectionEnabled) return;

  if (digitalRead(PIR_SENSOR) == HIGH) {
    if (millis() - lastMotionTime >= MOTION_DEBOUNCE_TIME) {
      bot.sendMessage(CHAT_ID, "Motion detected. Capturing photo...", "");
      captureAndSendPhoto(true);
      lastMotionTime = millis();
    }
  }
}

void checkScheduledCapture() {
  if (!timeCaptureEnabled) return;

  int h = timeClient.getHours();
  int m = timeClient.getMinutes();

  if (h == scheduledHour && m == scheduledMinute && millis() - lastCaptureTime > 60000UL) {
    bot.sendMessage(CHAT_ID, "Scheduled capture triggered.", "");
    captureAndSendPhoto(true);
    lastCaptureTime = millis();
  }
}

void checkTimedEasterEggs() {
  if (!timedEasterEggsEnabled) return;

  unsigned long epoch = timeClient.getEpochTime();
  if (epoch < 100000UL) return;

  long dayNumber = epoch / 86400L;
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();

  for (int i = 0; i < timedEggCount; i++) {
    if (h == timedEggs[i].hour &&
        m == timedEggs[i].minute &&
        timedEggs[i].lastDay != dayNumber) {
      timedEggs[i].lastDay = dayNumber;
      bot.sendMessage(CHAT_ID, timedEggs[i].message, "");
      captureAndSendPhoto(true);
      break;
    }
  }
}

// ===================== TIME CAPTURE =====================

void handleSetTime(String command) {
  String timeStr = command.substring(8);
  timeStr.trim();

  int colonIndex = timeStr.indexOf(':');

  if (colonIndex == -1) {
    bot.sendMessage(CHAT_ID, "Format: /settime HH:MM", "");
    return;
  }

  int newHour = timeStr.substring(0, colonIndex).toInt();
  int newMinute = timeStr.substring(colonIndex + 1).toInt();

  if (newHour < 0 || newHour > 23 || newMinute < 0 || newMinute > 59) {
    bot.sendMessage(CHAT_ID, "Invalid time. Use 24-hour format HH:MM.", "");
    return;
  }

  scheduledHour = newHour;
  scheduledMinute = newMinute;
  timeCaptureEnabled = true;

  saveSetting(EEPROM_SCHEDULED_HOUR, scheduledHour);
  saveSetting(EEPROM_SCHEDULED_MINUTE, scheduledMinute);
  saveSetting(EEPROM_TIME_ENABLED, 1);

  String msg = "Scheduled capture set to ";

  if (scheduledHour < 10) msg += "0";
  msg += String(scheduledHour);
  msg += ":";

  if (scheduledMinute < 10) msg += "0";
  msg += String(scheduledMinute);

  bot.sendMessage(CHAT_ID, msg, "");
}

// ===================== PHOTO =====================

void captureAndSendPhoto(bool saveToSD) {
  if (!cameraReady) {
    bot.sendMessage(CHAT_ID, "Camera is not ready.", "");
    return;
  }

  bot.sendMessage(CHAT_ID, "Capturing photo...", "");

  if (USE_FLASH_LED) {
    digitalWrite(FLASH_LED, HIGH);
    delay(120);
  }

  camera_fb_t* fb = esp_camera_fb_get();

  if (USE_FLASH_LED) {
    digitalWrite(FLASH_LED, LOW);
  }

  if (!fb) {
    bot.sendMessage(CHAT_ID, "Camera capture failed.", "");
    return;
  }

  if (saveToSD && sdAvailable) {
    savePhotoToSD(fb);
  }

  telegram_fb = fb;
  telegram_fb_index = 0;

  bool sent = bot.sendPhotoByBinary(
                CHAT_ID,
                "image/jpeg",
                fb->len,
                isMoreDataAvailable,
                getNextByte,
                nullptr,
                nullptr
              );

  telegram_fb = nullptr;
  telegram_fb_index = 0;

  esp_camera_fb_return(fb);

  bot.sendMessage(CHAT_ID, sent ? "Photo sent." : "Photo sending failed.", "");
}

void streamPhotos(int numPhotos) {
  bot.sendMessage(CHAT_ID, "Streaming " + String(numPhotos) + " photos...", "");

  for (int i = 0; i < numPhotos; i++) {
    captureAndSendPhoto(false);
    delay(1000);
  }
}

// ===================== SD FUNCTIONS =====================

void savePhotoToSD(camera_fb_t* fb) {
  if (!sdAvailable || !fb) return;

  String filename = "/photo_" + String(timeClient.getEpochTime()) + ".jpg";

  File file = SD.open(filename.c_str(), FILE_WRITE);

  if (!file) {
    Serial.println("Failed to save photo.");
    return;
  }

  file.write(fb->buf, fb->len);
  file.close();

  Serial.println("Saved: " + filename);
}

void listFiles() {
  if (!sdAvailable) {
    bot.sendMessage(CHAT_ID, "SD card is not available.", "");
    return;
  }

  File root = SD.open("/");

  if (!root) {
    bot.sendMessage(CHAT_ID, "Unable to open SD card root.", "");
    return;
  }

  String fileList = "SD card files:\n";
  int count = 0;

  File file = root.openNextFile();

  while (file) {
    fileList += String(file.name());
    fileList += "  ";
    fileList += String(file.size() / 1024);
    fileList += " KB\n";

    count++;

    if (fileList.length() > 3500) {
      bot.sendMessage(CHAT_ID, fileList, "");
      fileList = "";
    }

    file = root.openNextFile();
  }

  if (count == 0) {
    fileList += "No files found.\n";
  }

  bot.sendMessage(CHAT_ID, fileList, "");
}

void deleteFile(String filename) {
  if (!sdAvailable) {
    bot.sendMessage(CHAT_ID, "SD card is not available.", "");
    return;
  }

  if (filename.length() == 0) {
    bot.sendMessage(CHAT_ID, "Format: /delete filename.jpg", "");
    return;
  }

  if (!filename.startsWith("/")) {
    filename = "/" + filename;
  }

  if (SD.remove(filename.c_str())) {
    bot.sendMessage(CHAT_ID, "Deleted: " + filename, "");
  } else {
    bot.sendMessage(CHAT_ID, "Delete failed: " + filename, "");
  }
}

void clearFiles() {
  if (!sdAvailable) {
    bot.sendMessage(CHAT_ID, "SD card is not available.", "");
    return;
  }

  File root = SD.open("/");
  File file = root.openNextFile();

  int deleted = 0;

  while (file) {
    String path = String(file.name());
    file.close();

    if (!path.startsWith("/")) {
      path = "/" + path;
    }

    if (SD.remove(path.c_str())) {
      deleted++;
    }

    file = root.openNextFile();
  }

  bot.sendMessage(CHAT_ID, "Deleted files: " + String(deleted), "");
}

// ===================== STATUS =====================

void sendStatus() {
  String status = "System Status\n\n";

  status += "WiFi: ";
  status += WiFi.status() == WL_CONNECTED ? "Connected\n" : "Disconnected\n";

  status += "SSID: ";
  status += DEFAULT_SSID;
  status += "\n";

  status += "IP: ";
  status += WiFi.localIP().toString();
  status += "\n";

  status += "RSSI: ";
  status += String(WiFi.RSSI());
  status += " dBm\n";

  status += "Camera: ";
  status += cameraReady ? "Ready\n" : "Not ready\n";

  status += "PSRAM: ";
  status += psramFound() ? "Available\n" : "Not available\n";

  status += "SD card: ";
  status += sdAvailable ? "Available\n" : "Not available\n";

  status += "Free heap: ";
  status += String(ESP.getFreeHeap());
  status += " bytes\n";

  status += "Motion detection: ";
  status += motionDetectionEnabled ? "Enabled\n" : "Disabled\n";

  status += "Auto-send: ";
  status += autoSendEnabled ? "Enabled\n" : "Disabled\n";

  status += "Scheduled capture: ";
  status += timeCaptureEnabled ? "Enabled\n" : "Disabled\n";

  status += "Timed photo events: ";
  status += timedEasterEggsEnabled ? "Enabled\n" : "Disabled\n";

  status += "Scheduled time: ";
  if (scheduledHour < 10) status += "0";
  status += String(scheduledHour);
  status += ":";
  if (scheduledMinute < 10) status += "0";
  status += String(scheduledMinute);
  status += "\n";

  status += "Current time: ";
  status += timeClient.getFormattedTime();
  status += "\n";

  status += "Uptime: ";
  status += String((millis() - uptimeStart) / 1000);
  status += " seconds\n";

  bot.sendMessage(CHAT_ID, status, "");
}

void sendUptime() {
  bot.sendMessage(CHAT_ID, "Uptime: " + String((millis() - uptimeStart) / 1000) + " seconds", "");
}

void sendTemperature() {
  bot.sendMessage(CHAT_ID, "Chip temperature: " + String(temperatureRead()) + " °C", "");
}

// ===================== RESET =====================

void resetSettings() {
  motionDetectionEnabled = false;
  autoSendEnabled = false;
  timeCaptureEnabled = false;
  timedEasterEggsEnabled = true;
  scheduledHour = 7;
  scheduledMinute = 30;

  saveSetting(EEPROM_MOTION_ENABLED, 0);
  saveSetting(EEPROM_AUTO_SEND_ENABLED, 0);
  saveSetting(EEPROM_TIME_ENABLED, 0);
  saveSetting(EEPROM_TIMED_EGGS_ENABLED, 1);
  saveSetting(EEPROM_SCHEDULED_HOUR, 7);
  saveSetting(EEPROM_SCHEDULED_MINUTE, 30);

  bot.sendMessage(CHAT_ID, "Settings reset successfully.", "");
}

// ===================== FUN COMMANDS =====================

void sendFunList() {
  String msg = "Fun Commands\n\n";

  msg += "/rickroll\n";
  msg += "/whoami\n";
  msg += "/selfdestruct\n";
  msg += "/hacker\n";
  msg += "/ask question\n";
  msg += "/coffee\n";
  msg += "/weather\n";
  msg += "/secretcode\n";
  msg += "/helpme\n";
  msg += "/hiddenfeature\n";
  msg += "/warpdrive\n";
  msg += "/hackme\n";
  msg += "/password123\n";
  msg += "/scream\n";
  msg += "/snapme\n";
  msg += "/shutdown\n";
  msg += "/boostwifi\n";
  msg += "/paranormal\n";
  msg += "/secretadmin\n";
  msg += "/esp32\n";
  msg += "/fakeupdate\n";
  msg += "/darkmode\n";
  msg += "/battery100\n";
  msg += "/cheatcode\n";
  msg += "/coinflip\n";
  msg += "/roll\n";
  msg += "/rockpaperscissors rock\n";
  msg += "/rockpaperscissors paper\n";
  msg += "/rockpaperscissors scissors\n";
  msg += "/roastme\n";
  msg += "/braincheck\n";
  msg += "/genius\n";
  msg += "/motivation\n";
  msg += "/study\n";
  msg += "/focus\n";
  msg += "/iq\n";
  msg += "/smart\n";
  msg += "/lazy\n";
  msg += "/legend\n";

  bot.sendMessage(CHAT_ID, msg, "");
}

void playRockPaperScissors(String command) {
  String playerChoice = command.substring(String("/rockpaperscissors").length());
  playerChoice.trim();

  if (playerChoice != "rock" && playerChoice != "paper" && playerChoice != "scissors") {
    bot.sendMessage(CHAT_ID, "Format: /rockpaperscissors rock, paper, or scissors", "");
    return;
  }

  int r = random(0, 3);
  String botChoice = (r == 0) ? "rock" : ((r == 1) ? "paper" : "scissors");

  String result;

  if (playerChoice == botChoice) {
    result = "Draw.";
  } else if ((playerChoice == "rock" && botChoice == "scissors") ||
             (playerChoice == "paper" && botChoice == "rock") ||
             (playerChoice == "scissors" && botChoice == "paper")) {
    result = "You win.";
  } else {
    result = "You lose.";
  }

  String msg = "You: " + playerChoice + "\n";
  msg += "Bot: " + botChoice + "\n";
  msg += result;

  bot.sendMessage(CHAT_ID, msg, "");
}

// ===================== HELP =====================

void sendHelp() {
  String help = "Command List\n\n";

  help += "Camera:\n";
  help += "/photo - Capture and send a photo\n";
  help += "/stream N - Send N photos, maximum 20\n\n";

  help += "Automation:\n";
  help += "/motion on - Enable motion capture\n";
  help += "/motion off - Disable motion capture\n";
  help += "/autosend on - Enable hourly photo\n";
  help += "/autosend off - Disable hourly photo\n";
  help += "/settime HH:MM - Set daily capture time\n";
  help += "/timecapture on - Enable daily capture\n";
  help += "/timecapture off - Disable daily capture\n";
  help += "/timeeggs on - Enable timed photo events\n";
  help += "/timeeggs off - Disable timed photo events\n\n";

  help += "Storage:\n";
  help += "/list - List SD card files\n";
  help += "/delete filename.jpg - Delete file\n";
  help += "/clear - Delete all SD files\n\n";

  help += "System:\n";
  help += "/status - Show system status\n";
  help += "/uptime - Show uptime\n";
  help += "/temp - Show chip temperature\n";
  help += "/ip - Show IP address\n";
  help += "/mac - Show MAC address\n";
  help += "/reset - Reset saved settings\n\n";

  help += "Fun:\n";
  help += "/rick - Show fun commands\n";

  bot.sendMessage(CHAT_ID, help, "");
}
