# WaterSysPro ESP32 Controller

> **Примечание:** при взаимодействии с GitHub Copilot в этой сессии мы общаемся на русском языке.
>
Это репозиторий содержит прошивку и сопутствующие инструменты для
`WaterSysPro` системы водоподготовки на ESP32. Код реализует конечный
автомат (FSM), который управляет циклами заполнения, озонирования,
аэрации, отстаивания, фильтрации и обратной промывки, а также
веб-интерфейсом конфигурации и меню инженера.

## Features

* Dual ultrasonic level sensors with real‑time readings
* Configurable timers and schedules
* MQTT telemetry and remote control (optional)
* Event logging with persistent storage (SD card / SPIFFS)
* Full SD management directly from the web interface – list/download/delete files, prune old logs, export/rotate/format card; also available via REST endpoints (see docs/SD_INTEGRATION.md)
* Web UI assets can now live on the SD card to free internal flash (see docs/SD_INTEGRATION.md)
* Filterable log types via engineer menu or REST API
* Safe fallback to internal flash when SD fails
* Admin token protection and password‑secured engineer menu

## Event Logging

Logs are written as human-readable text lines to `/events.log` on the
mounted filesystem.  If a microSD card is present, logs go to the card;
if the card fails or is removed, the firmware automatically falls back to
SPIFFS with no loss of data.  Each log entry looks like:

```
DD.MM.YYYY HH:MM - type:XX param:YY lvl:Z
```

The field meanings are:

* `type` – event code (see `config.h` for definitions)
* `param` – optional numeric parameter
* `lvl` – log level (0=debug, 1=info, 2=warning, 3=error)

### Filtering

Only selected event categories are written.  The default filter includes
emergency events, watchdog resets, sensor errors and any error‑level
messages.  The mask can be altered from the engineer menu (`Log Filter`
entry) or via the `/api/log_filter` REST endpoint.  You may enable
additional categories such as `State Chg` or `Web/API` to capture more
information.

### Migration

Firmware automatically converts old binary-format logs to text upon
startup.  A one-time migration routine runs during SPIFFS or SD
initialization.

### Export

The engineer menu offers commands to view or export logs; exported files
are also plain text and can be copied off the card for analysis.

## Building & Flashing

Development uses PlatformIO.  To build and upload:

```sh
cd d:\WatersysPro
platformio run          # compile
platformio run -t upload # flash
```

Serial monitor is available via `platformio device monitor` (115200
baud).

## Notes for Developers

* Sensor polling period is configurable; the FSM now refreshes sensor
  readings every control loop invocation to ensure current values are
  always used.
* Logging functions live in `src/event_logging.cpp`; filtering and
  storage logic may be adjusted there.
* Add new menu items in `src/menu_data.cpp` and handlers in
  `src/engineer_menu.cpp`.

## Further Documentation

Inline comments throughout the C++ sources explain behavior and
constants.  See `include/config.h` for pin assignments and compile-time
options.

For assistance or additional documentation, modify this README or
contact the maintainer.

---

### Uploading UI files to the SD card

A helper script is provided at the project root that automates the usual
`mklittlefs`/`esptool` workflow.  It packages whatever is in the
`закинуть на сд/` directory into a SPIFFS image and flashes it to the board
so that on the next boot the firmware will copy the files to the card.

```sh
python upload_sd.py [COMx]
```

* the default serial port is `COM5`; override by passing a port name or
  setting the `SPIFFS_OFFSET` environment variable if your partition table
  differs from the stock `esp32dev` layout.
* requirements:
  * `mklittlefs` and `esptool.py` on your PATH (PlatformIO installations
    already include both).  The script will also look under
    `~/.platformio/packages` if they're not on PATH.
  * `pyserial` installed in the Python environment used to run the script
    (`pip install pyserial`).  This is only needed for the mini terminal that
    opens automatically after flashing.

After the flash finishes the script drops you into a serial monitor so you
can watch the log messages as the device mounts the card and moves the
files.

A GitHub Actions workflow (`.github/workflows/check_libs.yml`) runs on every
push and pull request.  It executes `scripts/analyze_libs.py`, which verifies
that every library declared in `lib_deps` is actually referenced by the
source.  The job fails if unused dependencies are detected, helping keep the
flash footprint under control.



* **SD-only firmware only:**
The project is configured to build without any SPIFFS image; all web UI and
static assets are expected to reside on a mounted SD card.  The single
`esp32dev` environment in `platformio.ini` disables SPIFFS (`board_build.filesystem = none`) and compiles with `-DUSE_SPIFFS=0`.