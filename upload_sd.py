#!/usr/bin/env python3
"""
Simple helper script to package the contents of the `закинуть на сд/`
directory into a LittleFS image and flash it to the ESP32's SPIFFS
partition.  After the device reboots it will copy those files onto an
inserted SD card; this is the easiest way to populate the card from a PC.

Usage:
    python upload_sd.py [COMx]

Requirements:
* `mklittlefs` must be installed and available on PATH.  On Windows you can
  use the version bundled with PlatformIO (``.pio\packages\tool-mklittlefs``).
* `esptool.py` must be installed (also bundled with PlatformIO).
* The board should be connected on a serial port (default COM5).

The script builds a 1 MiB image containing the web UI files and writes it to
0x300000, which is the default SPIFFS offset used by the stock esp32dev
partition table (this matches the `esp32dev` environment in the project).

After flashing the image the serial terminal is opened so you can watch the
firmware start and copy files to whatever SD card is attached to the
controller.
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.dirname(__file__))
webdir = os.path.join(ROOT, "закинуть на сд")
if not os.path.isdir(webdir):
    print(f"error: directory '{webdir}' not found")
    sys.exit(1)

spiffs_img = os.path.join(ROOT, "spiffs.bin")

# helper to find a program either on PATH or inside .pio packages

def find_tool(name, exe_name=None):
    """Locate an executable.

    * prefer whatever is on PATH
    * if not found, look under the user's global PlatformIO packages
      directory (`~/.platformio/packages`).
    """
    exe_name = exe_name or name
    path = shutil.which(exe_name)
    if path:
        return path
    # search global pio packages
    home = os.path.expanduser("~")
    glpkg = os.path.join(home, ".platformio", "packages")
    if os.path.isdir(glpkg):
        for sub in os.listdir(glpkg):
            if name in sub.lower():
                candidate = os.path.join(glpkg, sub, exe_name + ('.exe' if os.name=='nt' else ''))
                if os.path.isfile(candidate):
                    return candidate
    # fall back to plain name so user sees an error when executed
    return exe_name

import shutil

print("building LittleFS image from", webdir)
# 1MiB size, block 4KiB, page 256
mk = find_tool("tool-mklittlefs", "mklittlefs")
subprocess.check_call([
    mk,
    "-c", webdir,
    "-b", "4096",
    "-p", "256",
    "-s", "0x100000",
    spiffs_img,
])
print("image created:", spiffs_img)

port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
offset = os.environ.get("SPIFFS_OFFSET", "0x300000")

print(f"flashing to {port} at offset {offset} ...")
# try to locate esptool executable; if we only end up with "esptool.py" then
# invoke it via the python interpreter so the module path is searched.
esp = find_tool("esptool", "esptool.py")
if esp.endswith("esptool.py") or esp == "esptool.py":
    subprocess.check_call([
        sys.executable,
        "-m", "esptool",
        "--port", port,
        "write_flash",
        offset,
        spiffs_img,
    ])
else:
    subprocess.check_call([
        esp,
        "--port", port,
        "write_flash",
        offset,
        spiffs_img,
    ])

print("flash complete; launching serial terminal")
try:
    subprocess.call(["python", "-m", "serial.tools.miniterm", port, "115200"])
except KeyboardInterrupt:
    pass

print("done")
