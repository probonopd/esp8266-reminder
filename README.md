# esp8266-reminder [![Build Status](https://github.com/probonopd/esp8266-reminder/actions/workflows/compile.yml/badge.svg)](https://github.com/probonopd/esp8266-reminder/actions/workflows/compile.yml)

The reminder will remind the user to do certain tasks, based on events in a online calendar.
The user can confirm that tasks are done by pressing a button.

## Hardware

* WeMos NodeMCU ESP8266 board (or similar)
* Speaker or buzzer
* 16x2 lines display with i2c "backpack"

## Theory of operation

1. When the device is powered on, it connects to the WLAN. During this time, it shows "..." on the display
1. If it cannot connect to the WLAN, it opens an access point for configuration
1. Once it is connected successfully, it shows the IP address for a few seconds
1. It asks a Google Apps Script web app ("API") for any tasks that are due within the next 30 minutes time window
1. The API is defined in `Code.cs`, runs on demand on Google servers, and uses Google Calendar events as its backing store
1. If the API responds with at least one task, then it plays a rintone (in RTTTL format), switches the screen backlight on, and displays the name of the task
1. It repeats playing the ringtone in a configurable interval
1. Once the user confirms that the task is done by pressing the button, the task gets confirmed via the API
1. This results in a second event in the Google calendar with the same name as the original event plus "done" in the title and at the same time as the original event; in the description of that event one can see at which time the task was confirmed
1. Every half hour (at 0 minutes and at 30 minutes past the hour plus a few seconds), the device restarts and everything begins again
1. There is a web interface for configuration and for updating the firmware over the air

## Goals

* Ease of use: Only one button, no confusing messages for the user
* Robustness
* Simplicity

## System Requirements

Currently this works only on ESP8266, not on ESP32 due to this [issue](https://github.com/electronicsguy/HTTPSRedirect/issues/7).

## Downloading

From GitHub Actions under "Summary", "Artifacts"

## Flashing

E.g., on FreeBSD:

```
python3 -m pip install esptool
sudo -E python3 -m esptool erase_flash
sudo -E python3 -m esptool --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size 4MB  0x00000 firmware.bin && sudo screen /dev/ttyU0 115200
```

__NOTE:__ On FreeBSD, flashing with CH340 is [not working](https://github.com/espressif/esptool/issues/272). Use, e.g., a Prolific USB to Serial adapter instead.

## Pinout

| WeMos NodeMCU ESP8266 | Periperhal            |
|-----------------------|-----------------------|
| D0                    | Button (built in)     |
| D1                    | Display SCL           |
| D2                    | Display SDA           |
| VIN                   | Display VCC           |
| GND                   | Display GND           |
| GND                   | Speaker -             |
| D5                    | Speaker +             |

## Configuring

Configuration is done via the web using a simple text format. It can be entered into the device, or can be loaded from a URL that is entered instead:

```
# Will be advertised to the network under this name
name MyReminder

# The backend API URL
api https://script.google.com/macros/s/xxxxxxxxxxxxxxxxxx_xxxxxx_xxxxxxxxxxxxxxxxxxxxx-x_xxxxxxxxxxxxxxxxxxxxxx/exec

# Define a ringtone in RTTTL format
ringtone Entertainer:d=4,o=5,b=140:8d,8d#,8e,c6,8e,c6,8e,2c.6,8c6,8d6,8d#6,8e6,8c6,8d6,e6,8b,d6,2c6,p,8d,8d#,8e,c6,8e,c6,8e,2c.6,8p,8a,8g,8f#,8a,8c6,e6,8d6,8c6,8a,2d6

# Set the volume, 0-11
volume 11

# Set after how many seconds the ringtone is repeated, until the task is confirmed by pressing the button
repeat 15

```

## Debugging

Serial:

```
sudo screen /dev/ttyU0 115200
```

Telnet:

```
telnet hostname.local
```

## Ideas for future features

Contributions are welcome.

- [ ] Use a Pub/sub protocol (if we can make it as robust)
- [ ] Be able to trigger tasks at other times than every half hour
- [ ] Energy saving
- [ ] Play sounds and spoken messages via a i2s DAC
- [ ] Upload diagnostics
- [ ] Use a PIR sensor
- [ ] ...

Also see

- <https://github.com/SensorsIot/Reminder-with-Google-Calender/tree/master/ReminderV2> by Andreas Spiess
