# NiceMCU-C5E-DEV_2.8IPS

[中文](./README.md)

An Arduino board-level self-test and hardware validation example for the
`NiceMCU-C5E-DEV_2.8IPS` development board.

This project provides basic test functions for the ST7789 display, CST816D
capacitive touch controller, MicroSD card, WS2812 RGB LED, user button, and
buzzer. It can be used for power-on inspection, peripheral verification, and
as a reference for further development.

The interface is drawn directly with Adafruit GFX and **does not require
LVGL**.

## Interface Preview

![NiceMCU-C5E-DEV_2.8IPS interface preview](./docs/images/esp32-c5-ui-preview.png)

The current interface uses a lightweight card-style layout:

- The board model and example name are shown in the header
- Live touch coordinates are shown in the center
- Project attribution is shown in the footer
- Only the touch coordinate area is refreshed, reducing flicker caused by
  full-screen redraws

## Features

- ST7789 240 × 320 IPS display initialization and interface rendering
- CST816D capacitive touch controller detection
- Real-time touch `X / Y` coordinate reading and display
- MicroSD card initialization, capacity detection, and root directory listing
- Onboard WS2812 RGB LED rainbow animation
- User button control for enabling and disabling the RGB animation
- Buzzer startup melody
- Serial output for peripheral initialization and runtime status
- Display and RGB LED enabled before the startup melody
- MicroSD initialized last so a slow card does not delay visible startup

## Hardware and Software

| Item                  | Configuration          |
| --------------------- | ---------------------- |
| Development board     | NiceMCU-C5E-DEV_2.8IPS |
| MCU                   | ESP32-C5               |
| Development framework | Arduino                |
| Display controller    | ST7789                 |
| Display resolution    | 240 × 320              |
| Graphics library      | Adafruit GFX           |
| Touch controller      | CST816D                |
| Touch interface       | I2C                    |
| Storage               | MicroSD / SPI          |
| RGB LED               | WS2812                 |
| Serial baud rate      | 115200                 |
| GUI framework         | No LVGL                |

## Arduino Library Dependencies

Install the following libraries through the Arduino IDE Library Manager:

- `Adafruit GFX Library`
- `Adafruit ST7735 and ST7789 Library`
- `Adafruit NeoPixel`

The following libraries are provided by the Arduino-ESP32 board core:

- `SPI`
- `Wire`
- `SD`

## Verified Build Configuration

The current program has been compiled with the following Arduino IDE
configuration:

| Option          | Configuration      |
| --------------- | ------------------ |
| Board           | ESP32C5 Dev Module |
| Arduino-ESP32   | 4.0.0-alpha1       |
| CPU Frequency   | 240 MHz            |
| Flash Size      | 4 MB               |
| Flash Frequency | 80 MHz             |
| Upload Speed    | 921600             |
| PSRAM           | Disabled           |

A newer Arduino-ESP32 version with explicit ESP32-C5 support may also be used.
After upgrading the board core, perform a complete rebuild and validate the
result on the physical board.

## Getting Started

1. Install an Arduino-ESP32 board core that supports ESP32-C5.
2. Install the three Adafruit libraries listed above through Arduino IDE.
3. Open `ESP32_C5_2.8IPS.ino`.
4. Select `ESP32C5 Dev Module` as the board.
5. Select the serial port connected to the board.
6. Compile and upload the program.
7. Open Serial Monitor and set the baud rate to `115200`.

After uploading, the program starts in the following order:

```text
Initialize GPIO, WS2812, buzzer, and touch
→ Initialize the TFT
→ Draw the main interface and enable the display backlight
→ Turn on the WS2812 RGB LED
→ Play the startup melody
→ Initialize the MicroSD card
→ Enter the touch coordinate and RGB animation loop
```

The main interface remains visible while the MicroSD card is being initialized.
Live touch coordinate updates begin after `setup()` completes and the program
enters `loop()`.

## Pin Assignment

### TFT Display

| Function |     GPIO |
| -------- | -------: |
| TFT_DC   |        4 |
| TFT_CS   |        5 |
| TFT_CLK  |        6 |
| TFT_MOSI |        7 |
| TFT_RST  | Not used |
| TFT_BL   |       25 |

### CST816D Touch Controller

| Function |     GPIO |
| -------- | -------: |
| CTP_SCL  |       23 |
| CTP_SDA  |       24 |
| CTP_INT  |        1 |
| CTP_RST  | Not used |

The CST816D I2C address is `0x15`.

### MicroSD

| Function | GPIO |
| -------- | ---: |
| SD_CS    |   13 |
| SD_MOSI  |   10 |
| SD_SCK   |    9 |
| SD_MISO  |    8 |

### Other Peripherals

| Function    | GPIO |
| ----------- | ---: |
| WS2812 data |   27 |
| Buzzer      |   26 |
| User button |   28 |

These pin assignments apply only to the C5E development board used by this
project. Do not apply them directly to the `NiceMCU-32S-DEV_2.8IPS` or other
ESP32 boards.

## Serial Diagnostics

The program reports the following information through the serial port:

- CST816D response status
- ST7789 initialization status
- Buzzer melody start and completion
- MicroSD mount status
- MicroSD type, capacity, and root directory contents
- Touch coordinates and gesture value
- RGB animation status after a button press

## Usage Notes

- This project is a board-level self-test and hardware validation example, not
  complete product firmware.
- The current program does not use LVGL and does not include Wi-Fi, Bluetooth,
  or a multi-page application framework.
- GPIO assignments, display controllers, and touch controllers may differ
  between board revisions. Check the matching hardware documentation before
  modifying the program.
- If MicroSD mounting fails, first check the wiring and card format. For
  troubleshooting, a FAT32 card with a capacity of 32 GB or less is
  recommended.

