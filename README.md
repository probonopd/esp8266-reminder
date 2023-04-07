# esp8266-reminder [![Build Status](https://github.com/probonopd/esp8266-reminder/actions/workflows/compile.yml/badge.svg)](https://github.com/probonopd/esp8266-reminder/actions/workflows/compile.yml)

The reminder will remind the user to do certain tasks, based on events in a online calendar.
The user can confirm that tasks are done by pressing a button.

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
| D4                    | i2s DAC LRC           |
| D8                    | i2s DAC BCLK          |
| RX                    | i2s DAC DIN           |
| GND                   | i2s DAC GND           |
| VIN (3V3 = lower vol) | i2s DAC VCC           |

## Configuring

Configuration is done via the web using a simple text format. It can be entered into the device, or can be loaded from a URL that is entered instead:

```
# Will be advertised to the network under this name
name MyReminder

# The backend API URL
api https://script.google.com/macros/s/xxxxxxxxxxxxxxxxxx_xxxxxx_xxxxxxxxxxxxxxxxxxxxx-x_xxxxxxxxxxxxxxxxxxxxxx/exec
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

- [ ] ...

Also see

- <https://github.com/SensorsIot/Reminder-with-Google-Calender/tree/master/ReminderV2> by Andreas Spiess
