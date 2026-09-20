# TC001 Bridges

Firmware that turns the [Ulanzi TC001](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882)
smart pixel clock into a desk display for the Qt Bridges projects. It shows the time and weather, how
many stars and downloads the Rust and C# bridges have and how those numbers are trending, the
battery level, and a Qt logo screen saver, all on the clock's 8x32 LED matrix.

It is a [Zephyr](https://zephyrproject.org/) application. It talks to GitHub, crates.io, NuGet,
Open-Meteo and an NTP server directly over Wi-Fi, so no computer has to collect anything.

![The pixel-art logos and weather icons](docs/images/pixel-logos.png)

- [What it shows](#what-it-shows)
- [Getting started](#getting-started)
- [Using the clock](#using-the-clock)
- [Configuration](#configuration)
- [How it works](#how-it-works)
- [Project layout](#project-layout)
- [Development](#development)
- [Limitations](#limitations)
- [License](#license)

## What it shows

The clock is a set of apps. **Left** and **right** switch between them with a sideways slide. When
you arrive at an app its name appears once (`TIME`, `TOTAL`, `TREND`, `LOGO`, `BATTERY`), then its
pages follow. The **middle** button does whatever the app on display defines.

| App               | Shows                                                                            | Middle button                                    |
| ----------------- | -------------------------------------------------------------------------------- | ------------------------------------------------ |
| Clock and weather | the time, the date, and the weather with an icon and the temperature             | pauses the page rotation                         |
| Bridges total     | stars of the C# and Rust bridges, then downloads of the NuGet and crates.io packages, each with its logo and an icon | pauses the page rotation |
| Bridges trends    | how much each of those numbers changed over the last week (`W`) and month (`M`)  | pauses the page rotation                         |
| Screen saver      | the Qt logo, in green, in white, or with its colour drifting                     | changes between those three                      |
| Battery           | a battery symbol filled to the charge and the percentage; while charging, the right column becomes a yellow bar with a dark block travelling up it | (none) |
| Settings          | a `SETTINGS` card; the menu is described [below](#the-device-menu)               | opens the menu                                   |

A dim pixel in the top right corner shows that the page rotation is paused.

Which apps are shown, and the date and time format, are set from a web page
([below](#web-settings)); brightness and network setup are in the device menu.

## Getting started

### What you need

- A TC001. Its USB-C port carries both power and the serial connection (a CH340 bridge).
- A Zephyr workspace with the SDK installed. This project was built against a recent Zephyr `main`
  (4.4.x) and Zephyr SDK 1.0.1.
- The `ulanzi_tc001` board definition, which lives in the Zephyr tree (`boards/ulanzi/ulanzi_tc001`).
  It **must** describe the LED strip with a 3.2 MHz clock and 4 bits per LED bit
  (`bits-per-symbol = <4>`, one = `0xe`, zero = `0x8`). With the older 6.4 MHz, 8 bit encoding a
  frame is 6144 bytes, more than the ESP32's 4092 byte SPI DMA transfer limit, so every frame goes
  out in two pieces, and the gap between them occasionally makes the LEDs latch half a frame: flicker
  and stray pixels while animating.

### Build and flash

```console
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_BASE=~/zephyrproject/zephyr
west build -p auto -b ulanzi_tc001/esp32/procpu .
west flash --esp-device /dev/cu.usbserial-310   # use your port
```

Flashing this board is occasionally flaky ("serial data stream stopped"). Retrying, or a lower speed
with `--esp-baud-rate 115200`, works. The log is on the same port at 115200 baud, for example with
`python -m serial.tools.miniterm /dev/cu.usbserial-310 115200`.

### First start: give it a Wi-Fi network

Wi-Fi networks are stored on the clock, not compiled in. With none saved, the clock opens its own
access point and scrolls the details across the display:

```text
WIFI TC001-Bridges PW tc001bridges   192.168.4.1   MIDDLE: CANCEL
```

1. Join that network from a phone or computer.
2. Open `http://192.168.4.1/`.
3. Add your network's name and password (the password may be empty for an open network) and save.
   The page also lists the saved networks (up to eight), each with a *Forget* button.

The clock restarts and connects. Afterwards it can be asked to do this again from the device menu
(**NETWORK, NEW NET**).

The access point's name, password and address can be changed, see [Configuration](#configuration).

## Using the clock

### The device menu

Go to the last app, `SETTINGS`, and press the middle button. Left and right move between items and
the middle button chooses. The menu closes by itself after a minute without a button press.

```text
BRIGHTNESS   AUTO on/off  |  LEVEL (or, with AUTO on, BIAS)  |  BACK
NETWORK      SSID  |  IP  |  NEW NET  |  BACK
EXIT
```

- **LEVEL** is the brightness, 5 to 100 %, changed with left (`-`) and right (`+`) in steps of 5;
  the middle button confirms. With **AUTO** on, the display follows the light sensor and the same
  buttons shift it up or down by a **BIAS** of up to 50 points.
- **SSID** and **IP** show the network the clock is on and its address, scrolling when they are long.
  The address is what you open for the [web settings](#web-settings).
- **NEW NET** opens the setup access point described above, **next to** the clock's normal
  connection, so the clock keeps running. The middle button ends it and returns you to the menu. If
  nobody uses it, it closes by itself after 15 minutes.

Without a saved network (or when none is in range) the setup access point is opened at start-up
instead. There the middle button, or 15 minutes without a network being saved, restarts the clock so
it can try again.

### Web settings

Open the clock's address from the **NETWORK, IP** item in a browser on the same network. The page
switches the apps on and off, and chooses:

- the **date** format: `dd.mm.` or `mm/dd`,
- the **time** format: 24 hour, or 12 hour (with an `A` or `P` after it).

The Battery and Settings apps cannot be switched off. The page never offers Wi-Fi networks or
passwords: that form is only served on the clock's own access point (the clock decides by the address
a request arrived on), so nobody else on your network can read or replace them.

### Trends

Trends compare today's number with a daily record kept in flash (40 days). A change shows as `--`
until the record reaches back far enough, so the week pages fill in after a week and the month pages
after a month.

### Battery

The percentage is an estimate from the battery voltage, and whether it is charging is inferred from
that voltage too, because the clock has no signal for it (see [Limitations](#limitations)).

## Configuration

Build-time options are in `Kconfig`; set them in `prj.conf`, or in a `local.conf` next to it (ignored
by git) with lines such as `CONFIG_TC001_STATS_REFRESH_S=600`.

| Option                              | Default           | What it does                                                              |
| ----------------------------------- | ----------------- | ------------------------------------------------------------------------- |
| `CONFIG_TC001_AP_SSID`              | `TC001-Bridges`   | name of the setup access point                                            |
| `CONFIG_TC001_AP_PSK`               | `tc001bridges`    | its WPA2 password (8 to 63 characters; empty for an open network)         |
| `CONFIG_TC001_AP_IP`                | `192.168.4.1`     | its address, and the address of its setup page                            |
| `CONFIG_TC001_STATS_REFRESH_S`      | `300`             | how often stars and downloads are fetched; keep it at 120 or more, GitHub allows 60 unauthenticated requests an hour and a refresh uses two |
| `CONFIG_TC001_BRIGHTNESS`           | `40`              | the brightness (percent) the clock starts with, before anything is saved  |
| `CONFIG_TC001_LDR_DARK_COUNT`       | `3000`            | light sensor count that gives the dimmest automatic brightness            |
| `CONFIG_TC001_LDR_BRIGHT_COUNT`     | `150`             | light sensor count that gives full automatic brightness                   |
| `CONFIG_TC001_BATTERY_UV_PER_COUNT` | `1980`            | battery voltage per ADC count, in microvolts: the battery calibration     |

**Automatic brightness** maps the light sensor onto 5 to 100 % on a logarithmic scale between the two
counts. In a normal evening room the sensor reads about 400, and this clock's sensor reads a *lower*
count in more light. If the display gets dimmer when you shine a torch on it, swap the two values.

Everything the user can change at run time (apps, brightness, screen saver mode, formats) is saved in
flash and survives a restart. Changing the layout of those settings in a new firmware version
discards the old ones once, so they go back to their defaults.

## How it works

### Data

| What                    | Where from                                                             | How often        |
| ----------------------- | ---------------------------------------------------------------------- | ---------------- |
| Stars                   | `api.github.com/repos/qt/qtbridge-rust` and `qtbridge-csharp`          | 5 minutes        |
| Rust downloads          | crates.io, crate `qtbridge`                                            | 5 minutes        |
| C# downloads            | NuGet, `QtGroup.Qt.Bridge.CSharp.win-x64` plus `...linux-x64`          | 5 minutes        |
| Time                    | NTP (`pool.ntp.org`), kept in the clock's DS1307 RTC for the next start | hourly           |
| Location                | `ipwho.is`, from the clock's public IP address                         | 12 hours         |
| Weather and time zone   | Open-Meteo for that location (`timezone=auto` gives the offset, daylight saving included) | 30 minutes |

Failed requests are retried sooner and never blank the display: it keeps its last value. Clone
counts are not shown, because GitHub only reports them to repository admins.

Every request is HTTPS (TLS 1.2). A connection is checked against the one root certificate for its
server, embedded in the firmware (`certs/`, see [certs/README.md](certs/README.md)). The responses are
several kilobytes but the firmware only needs a few numbers, so `src/resp_scan.c` picks them out of
the stream as it arrives instead of parsing a document.

### Threads

- **main** runs the network tasks (time, location, weather, stats) one after the other, each with its
  own interval and retry delay. TLS memory allows only one connection at a time.
- **ui** owns the display. It ticks every 40 ms, asks the current app what to draw, slides pages and
  apps in, and follows the brightness. The other threads only change state that it reads.
- **HTTP server** serves the web pages.
- **input callbacks** turn the three buttons into `ui_step()` and `ui_middle()`.
- The **system work queue** starts and stops the setup access point.

### Display

There is no graphics library. `src/gfx.c` draws into a 768 byte framebuffer (text, sprites, masks,
slides) and sends it to the LEDs through Zephyr's `display` API, scaled by the brightness.

- Text uses a 3x5 pixel font. `fonts/3x5MatrixDisplay.ttf` is turned into `src/font3x5.c` by
  `tools/gen_font.py`.
- The logos and weather icons are hand-drawn 8x8 pixel art. `tools/pixel_logos.py` holds them as
  ASCII grids and generates `src/logos.c` and the preview above.
- The Qt logo (`src/qt_logo.c`) and the small star, arrow and battery shapes are drawn as text masks.

### What is stored in flash

Zephyr's settings (on NVS, in the `storage` partition) hold: the user settings, up to eight Wi-Fi
networks (unencrypted), the last known location and time zone offset, and the 40 day record of the
numbers for the trends.

## Project layout

```text
src/               the firmware
  main.c             start-up, the button handler, the network task loop
  ui.c, app.h        the display thread and the interface an app implements
  app_*.c            the apps: clock, bridges (total), trends, screensaver, battery, settings
  gfx.c, font3x5.c   drawing, the font; logos.c and qt_logo.c, icons.c hold the art
  stats.c, weather.c, location.c, clock.c   the network data and the time
  https.c, resp_scan.c                      TLS requests and number extraction
  wifi.c, setup.c, portal.c                 Wi-Fi, the setup access point, the web pages
  config.c, history.c                       what is saved in flash
  battery.c, light.c, brightness*.c         the two analog inputs and the brightness
  format.c, timeconv.c, scroll.c, ...       small pure helpers
tests/unit/        unit tests of those helpers
tools/             generators for the font and the logos
fonts/  certs/     the font source, the trusted root certificates
docs/images/       the logo preview
Kconfig  prj.conf  options
project-requirements.md   what the clock should do
```

## Development

### Adding an app

1. Write `src/app_xxx.c` implementing `struct app` from `src/app.h`: an optional `title`, `enter()`,
   `update()` (which draws into the frame it is given and says whether to redraw or slide), and
   optionally `middle()` and `nav()`.
2. Add an entry to `enum app_id` in `src/config.h` (the order is the order the apps rotate in), and
   register it in `src/ui.c`.
3. Add the file to `CMakeLists.txt`. If the user should be able to switch it off, add it to the list
   in `src/portal.c`; otherwise add it to `APPS_ALWAYS_ON` in `src/config.h`.

### Tests

`tests/unit` holds ztest suites for the logic that does not need hardware: number extraction from
responses, the web form parser, count, time, date and temperature formatting, the font table, colours,
weather code mapping, time conversion, the history record, the battery curve, brightness and
scrolling. Run them on a Linux host with `west twister -p native_sim -T tests/unit`; `native_sim` is
not available on macOS.

### Regenerating the font and logos

```console
pip install fonttools pillow
python3 tools/gen_font.py        # fonts/3x5MatrixDisplay.ttf -> src/font3x5.c
python3 tools/pixel_logos.py     # the ASCII art -> src/logos.c, src/logos.h, docs/images/pixel-logos.png
```

Edit the art in `tools/pixel_logos.py`, not in the generated files.

## Limitations

- **Battery.** The battery voltage is read through a resistor divider, and the ESP32's ADC is not very
  linear, so the percentage is an estimate (`CONFIG_TC001_BATTERY_UV_PER_COUNT`). The clock has no
  charging signal, so "charging" is inferred: near full voltage, or rising over ten minutes. It can be
  wrong for a while, for example just after unplugging a fully charged battery.
- **Trends** need history to build up, see above.
- **Certificates.** Their expiry dates are not checked, because the clock has no trusted time at
  start-up; the chain, signatures and host name are. The GlobalSign root used for crates.io expires in
  March 2029 and will need replacing (see `certs/README.md`).
- **Passwords** for saved Wi-Fi networks are stored unencrypted in flash.
- **Location** comes from the public IP address, so a VPN can put the clock in the wrong time zone.
- **Memory.** About 93 % of the ESP32's data RAM is in use, most of it Wi-Fi and TLS. New features
  need to be weighed against that.

## License

This project is free software: you can redistribute it and modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version. See [LICENSE](LICENSE). Each source file says so in its
`SPDX-License-Identifier: GPL-3.0-or-later` header.

The firmware is built on Zephyr and its modules, including mbedTLS, which are licensed under
Apache-2.0; the FSF regards that license as compatible with GPL version 3.

Weather data is by [Open-Meteo.com](https://open-meteo.com/) (CC BY 4.0). Locations come from
[ipwho.is](https://ipwho.is/), stars from the GitHub API, and download counts from crates.io and NuGet.
