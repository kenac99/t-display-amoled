# T-Display S3 AMOLED Retro 7 Segment LED Clock

A retro 80s-style 7-segment LED clock for the LilyGo T-Display S3 AMOLED (1.91" model). Features web-based configuration, automatic sunrise/sunset brightness, and proper battery charging.

![T-Display S3 AMOLED Flip Clock](images/clock.jpg)

## Features

- **Retro 7-Segment Display** - Classic 80s LED clock aesthetic using DS-Digital font
- **Web Configuration** - Change all settings from your browser at `http://clock.local`
- **Auto Brightness** - Dims at sunset, brightens at sunrise based on your location
- **Touch Controls** - Tap screen to cycle brightness modes (Auto → Full → Dim → Medium)
- **Battery Charging** - BQ25896 with float voltage for battery longevity
- **WiFi Reconnection** - Automatically reconnects if connection drops
- **NVS Persistence** - Settings survive reboots

## Hardware

- [LilyGo T-Display S3 AMOLED 1.91"](https://www.lilygo.cc/products/t-display-s3-amoled)
- 3.7V LiPo battery (tested with 3000mAh)

## Setup

### 1. Install PlatformIO

Install [PlatformIO](https://platformio.org/) in VS Code or use the CLI.

### 2. Clone and Configure

```bash
git clone https://github.com/kenac99/t-display-amoled.git
cd t-display-amoled
```

Edit `src/main.cpp` and update the defaults in the `ClockConfig` struct:

```cpp
// WiFi
char wifi_ssid[32] = "YOUR_WIFI_SSID";
char wifi_pass[64] = "YOUR_WIFI_PASSWORD";

// Location (for sunrise/sunset calculation)
float latitude = 37.7749;   // Your latitude
float longitude = -122.4194; // Your longitude
```

### 3. Build and Flash

```bash
pio run -t upload
```

### 4. Configure via Web

Once connected to WiFi, visit `http://clock.local` to configure:
- Display options (seconds, date, color scheme)
- Brightness settings (day/night levels, transition time)
- Location and timezone
- WiFi credentials

## Color Schemes

- Red (default)
- Green
- Blue
- White
- Amber

## Battery Notes

The BQ25896 is configured for:
- 1A charge current (safe for most LiPo cells 1000mAh+)
- 4.208V charge voltage
- Proper VINDPM and input current limits

### Float Voltage for Longevity

LiPo cells degrade faster when held at full charge (4.2V). To extend battery life, the clock implements float voltage management:

1. **Charging** (`chg`) - Charges to 4.2V normally
2. **Float** (`flt`) - After charge completes, drops to 4.0V float
3. **Recharge** - When battery falls below 3.9V, restores full charging

This keeps the battery around 80% when plugged in, significantly extending cycle life while still providing plenty of runtime if unplugged.

Battery percentage uses smoothed voltage readings (3.2V = 0%, 4.2V = 100%) with an exponential moving average to filter ADC noise.

## License

MIT

## Credits

- [LilyGo](https://www.lilygo.cc/) for the hardware
- [DS-Digital Font](https://www.dafont.com/ds-digital.font) by Dusit Supasawat
- [LVGL](https://lvgl.io/) graphics library
- [sunset](https://github.com/buelowp/sunset) by Peter Buelow - sunrise/sunset calculation approach
