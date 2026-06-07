# Triaxial Numechron Redux

This sketch is specifically for keeping the Triaxial Numechron synchronized with NTP. It manages the mechanical clock movement by synchronizing with internet time.

## Setup

### WiFi Configuration

To set up your WiFi credentials:

1. Copy `triaxial_numechron_redux/config.h.example` to `triaxial_numechron_redux/config.h`
2. Edit `triaxial_numechron_redux/config.h` and update with your WiFi credentials:
   ```cpp
   #define WIFI_SSID1 "your_wifi_ssid"
   #define WIFI_PASS1 "your_wifi_password"
   ```

**Note:** The `config.h` file is ignored by git to prevent accidentally committing your WiFi credentials.

### Required Libraries

This sketch requires the following libraries:
- ESPAsyncWebServer (by ESP32Async, formerly me-no-dev), which in turn installs AsyncTcp

These libraries may be installed through the Arduino IDE "Tools / Manage libraries" command.

## Usage

Once powered up, the clock will advance the minutes by one. After connecting to WiFi, it will synchronize with NTP. You'll need to adjust the dials using the following procedure:

1. Set the hour dial to the current hour. If the tens digit is displaying a number less than the current time, set to the current hour minus one.
2. Roll the tens digit forward to the current time's tens digit. If the minute window is displaying a number less than the current time, set to the current tens minus one.
3. Log in to http://tn.local and first adjust the minute digit to fit within the window. Then use the +1m and +5m buttons to roll the minutes forward until the time is correct.

## Web Interface

The clock provides a web interface at http://tn.local (when mDNS is working) or at the device's IP address.

## Calibration

Over time, the minute display may drift from what the clock thinks it's showing due to slight mechanical imprecision in the stepper/gear train. The web interface includes a calibration feature to correct for this.

**Important:** Wait a minute or two after boot for the clock to settle and sync with NTP before starting calibration.

### Procedure

1. Use the alignment buttons to nudge the display until it's showing the correct time.
2. Press **"Display Is Correct"** to mark the starting point.
3. Wait — days or weeks — until visible drift appears.
4. Use the alignment buttons to nudge the display back to the correct position.
5. Press **"Display Is Correct"** again. The clock calculates the correction (nudge steps ÷ elapsed minutes) and saves it to flash.

The correction is automatically applied to every subsequent minute advance. The longer you wait between steps 2 and 4, the more precise the correction will be. After saving, the clock immediately begins a new measurement period, so you can refine the correction over time by repeating steps 3–5.
