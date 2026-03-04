#!/usr/bin/env python3
"""Create a ZIP of the entire project and copy it to an SD card path.

Usage:
    python scripts/archive_project.py /path/to/sd/card [foldername]

If the destination folder does not exist it will be created.  The resulting
archive will be named `watersyspro-<timestamp>.zip` and placed inside the
folder specified (by default a subfolder `backups` on the card).

Example:
    python scripts/archive_project.py "E:\" backups

This script is intended to be run on the development machine; it does not
mount or interact with the card itself beyond copying the archive file to
the given pathname.  Ensure the SD card is mounted and reachable before
running.
"""

import os
import sys
import zipfile
import datetime

if len(sys.argv) < 2:
    print("usage: python scripts/archive_project.py <sd-card-root> [subfolder]")
    sys.exit(1)

card_root = sys.argv[1]
subfolder = sys.argv[2] if len(sys.argv) > 2 else "backups"

target_dir = os.path.join(card_root, subfolder)
if not os.path.isdir(target_dir):
    os.makedirs(target_dir, exist_ok=True)

now = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
archive_name = f"watersyspro-{now}.zip"
archive_path = os.path.join(target_dir, archive_name)

root = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

print(f"Creating archive {archive_path} from {root}...")
with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as zf:
    for dirpath, dirnames, filenames in os.walk(root):
        # skip .git and build directories to reduce size
        if ".git" in dirpath or ".pio" in dirpath or "build" in dirpath:
            continue
        for fname in filenames:
            if fname.endswith(('.zip', '.bin')):
                continue
            path = os.path.join(dirpath, fname)
            arcname = os.path.relpath(path, root)
            zf.write(path, arcname)
print("Archive created successfully.")
