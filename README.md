# XIAO ESP32S3 Sense Telegram Camera Bot

A Telegram-controlled camera bot using the **Seeed Studio XIAO ESP32S3 Sense**.
It can capture photos, save them to a microSD card, send photos to Telegram, detect motion using a PIR sensor, send scheduled photos, and run fun Telegram commands.

---

## Features

* Capture photo from Telegram using `/photo`
* Send photo directly to Telegram
* Save photo to microSD card
* List, delete, and clear SD card files
* PIR motion-triggered photo capture
* Hourly automatic photo sending
* Daily scheduled photo capture
* Timed fun photo events
* System status command
* Uptime, temperature, IP, and MAC address commands
* Fun command list through `/rick`
* No deep sleep
* No reboot loop
* No Telegram Wi-Fi change command for stability

---

## Required Components

| Component                       | Required    | Notes                                |
| ------------------------------- | ----------- | ------------------------------------ |
| Seeed Studio XIAO ESP32S3 Sense | Yes         | Main board with camera support       |
| USB-C data cable                | Yes         | Must support data, not charging only |
| Wi-Fi connection                | Yes         | 2.4 GHz Wi-Fi recommended            |
| Telegram account                | Yes         | Needed to control the bot            |
| microSD card                    | Recommended | Used for saving photos               |
| microSD card reader             | Optional    | For checking files on computer       |
| PIR motion sensor               | Optional    | Needed only for motion detection     |
| Breadboard/jumper wires         | Optional    | Needed for PIR sensor                |
| External LED                    | Optional    | Flash LED is disabled by default     |

---

## Important Board Note

This project is for:

```text
Seeed Studio XIAO ESP32S3 Sense
```

It is **not** for:

```text
XIAO ESP32C3
ESP32-C3
Arduino Uno
Normal ESP32 board without camera
```

The XIAO ESP32S3 Sense has the camera module support required for this project.

---

## Arduino IDE Setup

### 1. Install Arduino IDE

Download and install Arduino IDE from:

```text
https://www.arduino.cc/en/software
```

---

### 2. Add ESP32 Board Package

Open Arduino IDE.

Go to:

```text
Arduino IDE → Settings / Preferences
```

Find:

```text
Additional Boards Manager URLs
```

Add this URL:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Click **OK**.

---

### 3. Install ESP32 Board

Go to:

```text
Tools → Board → Boards Manager
```

Search:

```text
esp32
```

Install:

```text
esp32 by Espressif Systems
```

---

### 4. Select Board

Go to:

```text
Tools → Board → ESP32 Arduino
```

Select:

```text
XIAO_ESP32S3
```

or:

```text
Seeed XIAO ESP32S3
```

---

## Arduino IDE Board Settings

Use these settings:

```text
Board: Seeed XIAO ESP32S3 / XIAO_ESP32S3
USB CDC On Boot: Enabled
PSRAM: OPI PSRAM
Partition Scheme: Huge APP / Maximum APP
Upload Speed: 115200
```

Very important:

```text
PSRAM must be enabled.
```

If PSRAM is disabled, the camera may show:

```text
Camera capture failed
```

---

## Required Arduino Libraries

Install these from:

```text
Sketch → Include Library → Manage Libraries
```

Install:

```text
UniversalTelegramBot by Brian Lough
ArduinoJson by Benoit Blanchon
NTPClient by Fabrice Weinberg
```

The following libraries come with the ESP32 board package:

```text
WiFi
WiFiClientSecure
esp_camera
SD
SPI
EEPROM
WiFiUdp
```

---

# Telegram Bot Setup

## 1. Create a Telegram Bot

Open Telegram.

Search for:

```text
@BotFather
```

Open BotFather and send:

```text
/newbot
```

BotFather will ask for a bot name.

Example:

```text
XIAO Camera Bot
```

Then BotFather will ask for a username.
The username must end with `bot`.

Example:

```text
xiao_camera_project_bot
```

BotFather will give you a bot token.

It looks like this:

```text
1234567890:ABCDEF_your_token_here
```

Save this token carefully.

---

## 2. Start Your Bot

Open your newly created Telegram bot.

Press:

```text
Start
```

or send:

```text
/start
```

This is required. The ESP32 cannot message you unless you have started the bot once.

---

## 3. Get Your Telegram Chat ID

Search Telegram for:

```text
@getmyid_bot
```

or:

```text
@userinfobot
```

Start the bot.

It will show your Telegram user ID.

Example:

```text
987654321
```

This number is your `CHAT_ID`.

---

## 4. Add Telegram Details to Code

In the code, replace:

```cpp
const char* BOT_TOKEN = "YOUR_TELEGRAM_BOT_TOKEN";
const char* CHAT_ID   = "YOUR_TELEGRAM_CHAT_ID";
```

with your real values:

```cpp
const char* BOT_TOKEN = "1234567890:ABCDEF_your_token_here";
const char* CHAT_ID   = "987654321";
```

---

# Wi-Fi Setup

Replace:

```cpp
const char* DEFAULT_SSID     = "YOUR_WIFI_NAME";
const char* DEFAULT_PASSWORD = "YOUR_WIFI_PASSWORD";
```

with your Wi-Fi details:

```cpp
const char* DEFAULT_SSID     = "MyHomeWiFi";
const char* DEFAULT_PASSWORD = "MyPassword123";
```

Use a 2.4 GHz Wi-Fi network.
Many ESP32 boards do not work with 5 GHz Wi-Fi.

---

# Wiring

## Basic Setup

For basic camera and Telegram use:

```text
No wiring needed.
Only connect XIAO ESP32S3 Sense using USB-C.
```

---

## Optional PIR Motion Sensor Wiring

| PIR Sensor | XIAO ESP32S3 Sense                  |
| ---------- | ----------------------------------- |
| VCC        | 5V or 3.3V, depending on PIR module |
| GND        | GND                                 |
| OUT        | D0 / GPIO1                          |

The code uses:

```cpp
#define PIR_SENSOR 1
```

---

## Optional External Flash LED

Flash is disabled by default:

```cpp
#define USE_FLASH_LED false
```

If you connect an external LED to GPIO4, change:

```cpp
#define USE_FLASH_LED true
```

Use a proper resistor with the LED.

---

# Uploading the Code

## 1. Connect Board

Connect the XIAO ESP32S3 Sense to your computer using a USB-C data cable.

---

## 2. Select Port

In Arduino IDE:

```text
Tools → Port
```

Select the port that appears after connecting the board.

On Mac it may look like:

```text
/dev/cu.usbmodem...
```

On Windows it may look like:

```text
COM3
COM4
```

---

## 3. Upload

Click the **Upload** button.

If upload fails:

1. Hold the **BOOT** button.
2. Press and release **RESET**.
3. Release **BOOT**.
4. Upload again.

---

## 4. Open Serial Monitor

After upload:

```text
Tools → Serial Monitor
```

Set baud rate:

```text
115200
```

You should see:

```text
XIAO ESP32S3 Sense Telegram Camera Bot Starting...
WiFi connected.
Camera initialized.
SD card initialized.
```

The Telegram bot should send:

```text
XIAO ESP32S3 Sense Camera Bot is online.
```

---

# First Test

Send this command in Telegram:

```text
/photo
```

Expected result:

```text
Capturing photo...
Photo sent.
```

If it says:

```text
Camera capture failed
```

check:

```text
PSRAM: OPI PSRAM
Camera ribbon/module properly attached
Correct board selected
No loose connection
```

---

# Commands

## Main Commands

| Command     | Function                    |
| ----------- | --------------------------- |
| `/photo`    | Capture and send one photo  |
| `/stream N` | Send N photos, maximum 20   |
| `/status`   | Show complete system status |
| `/help`     | Show command list           |
| `/rick`     | Show fun commands           |

---

## Automation Commands

| Command            | Function                      |
| ------------------ | ----------------------------- |
| `/motion on`       | Enable PIR motion capture     |
| `/motion off`      | Disable PIR motion capture    |
| `/autosend on`     | Send one photo every hour     |
| `/autosend off`    | Disable hourly photo          |
| `/settime HH:MM`   | Set daily photo time          |
| `/timecapture on`  | Enable daily scheduled photo  |
| `/timecapture off` | Disable daily scheduled photo |
| `/timeeggs on`     | Enable timed photo events     |
| `/timeeggs off`    | Disable timed photo events    |

Example:

```text
/settime 07:30
/timecapture on
```

This sends one photo every day at 7:30 AM.

---

## SD Card Commands

| Command                | Function              |
| ---------------------- | --------------------- |
| `/list`                | List files on SD card |
| `/delete filename.jpg` | Delete one file       |
| `/clear`               | Delete all files      |

Example:

```text
/delete photo_1712345678.jpg
```

---

## System Commands

| Command   | Function              |
| --------- | --------------------- |
| `/uptime` | Show running time     |
| `/temp`   | Show chip temperature |
| `/ip`     | Show IP address       |
| `/mac`    | Show MAC address      |
| `/reset`  | Reset saved settings  |

The code intentionally does not provide Telegram reboot, deep sleep, or Wi-Fi change commands. This avoids accidental reboot loops and unstable behavior.

---

# Timed Photo Events

When `/timeeggs on` is enabled, the bot automatically takes photos at selected fun times.

Examples:

```text
00:00 - Midnight photo
03:00 - 3 AM photo
13:37 - Leet photo
23:59 - Last photo before tomorrow
```

There are 30 timed photo events in the code.

Each event runs only once per day.

---

# Fun Commands

Use:

```text
/rick
```

to see the fun commands.

Available fun commands include:

```text
Command	Description
/rickroll	Sends the classic Rick Astley lyric.
/whoami	Gives a humorous roast about the user.
/selfdestruct	Simulates a self-destruct countdown.
/hacker	Pretends to hack into a secure system.
/ask <question>	Magic 8-ball style random answer.
/coffee	Generates a virtual cup of coffee.
/weather	Reports "sunny in cyberspace."
/secretcode	Pretends a secret feature was unlocked.
/helpme	Gives a special response. At 3 AM it tells you to sleep.
/hiddenfeature	Claims to reveal a classified feature.
/warpdrive	Attempts to activate warp drive.
/hackme	Pretends the user has been hacked.
/password123	Jokes about weak passwords.
/scream	Sends a dramatic scream message.
/snapme	Pretends the camera is watching you.
/shutdown	Pretends to shut down but stays online.
/boostwifi	Claims to increase Wi-Fi power.
/paranormal	Checks for ghost activity.
/secretadmin	Pretends to enter administrator mode.
/esp32	Declares loyalty to ESP32 hardware.
/fakeupdate	Simulates a software update.
/update	Alias for /fakeupdate.
/darkmode	Pretends to activate dark mode.
/battery100	Reports a perfect battery level.
/cheatcode	Pretends to unlock special powers.
/coinflip	Flips a virtual coin.
/roll	Rolls a six-sided die.
/rockpaperscissors rock	Play Rock-Paper-Scissors.
/rockpaperscissors paper	Play Rock-Paper-Scissors.
/rockpaperscissors scissors	Play Rock-Paper-Scissors.
/roastme	Sends a random roast.
/braincheck	Performs a fake intelligence scan.
/genius	Pretends to evaluate genius level.
/motivation	Sends a sarcastic motivational message.
/study	Pretends to activate study mode.
/focus	Measures fake concentration level.
/iq	Pretends to calculate IQ.
/smart	Gives a humorous smartness evaluation.
/lazy	Gives a humorous laziness evaluation.
/legend	Checks whether the user qualifies as a legend.
```

These are harmless joke commands.

---


# Troubleshooting

## `UniversalTelegramBot.h: No such file or directory`

Install:

```text
UniversalTelegramBot by Brian Lough
```

from Library Manager.

---

## `esp_camera.h: No such file or directory`

Install the ESP32 board package and select:

```text
Seeed XIAO ESP32S3
```

Do not select Arduino Uno or ESP32-C3.

---

## Camera capture failed

Check:

```text
PSRAM: OPI PSRAM
Partition Scheme: Huge APP
Camera module connected properly
Correct board selected
```

Also ensure the code uses QVGA settings for stability.

---

## Bot does not reply

Check:

```text
Correct BOT_TOKEN
Correct CHAT_ID
Bot was started using /start
Wi-Fi is connected
Telegram is not blocked by network
```

---

## Board keeps restarting

Use:

```text
Erase All Flash Before Sketch Upload: Enabled
```

for one upload.

Then set it back to:

```text
Disabled
```

Also make sure there is no deep sleep or reboot command in the code.

---

## Wrong Wi-Fi keeps connecting

This happens if old Wi-Fi was saved in flash memory.

Fix:

```text
Tools → Erase All Flash Before Sketch Upload → Enabled
```

Upload once.

Then set it back to Disabled.

---

# Recommended First-Time Upload Settings

For the first upload:

```text
Board: Seeed XIAO ESP32S3
USB CDC On Boot: Enabled
PSRAM: OPI PSRAM
Partition Scheme: Huge APP / Maximum APP
Upload Speed: 115200
Erase All Flash Before Sketch Upload: Enabled
```

After first successful upload:

```text
Erase All Flash Before Sketch Upload: Disabled
```

---

# License

This project is intended for learning, experimentation, and personal use.



---

# Credits

Created for a XIAO ESP32S3 Sense Telegram camera project.

Made by Akshaj.
