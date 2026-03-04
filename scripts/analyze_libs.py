#!/usr/bin/env python3
"""Analyze which libraries declared in platformio.ini are actually
referenced by the C++ sources.

Usage: python scripts/analyze_libs.py

This script reads the `lib_deps` sections from the top-level
`platformio.ini` and then searches all `.cpp` and `.h` files in the
workspace for `#include` lines containing a substring of the library
name.  It's a heuristic tool: a library is considered "used" if one of
its name components appears in an include.  Unused libraries are
printed so you can remove them from `platformio.ini`.

The check is not foolproof, but it's a quick way to spot obvious
candidates for deletion when trying to shrink flash usage.
"""

import configparser
import os
import re
import sys

root = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))
ini = os.path.join(root, "platformio.ini")

config = configparser.ConfigParser()
config.read(ini)

libs = []
for section in config.sections():
    if config.has_option(section, "lib_deps"):
        # collect each line under lib_deps
        raw = config.get(section, "lib_deps")
        for line in raw.splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                libs.append(line)

if not libs:
    print("No lib_deps entries found.")
    sys.exit(0)

print("Libraries declared in platformio.ini:")
for lib in libs:
    print("  ", lib)

# search code
used = set()
pattern = re.compile(r'^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]')

for dirpath, _, filenames in os.walk(root):
    for fname in filenames:
        if fname.endswith(('.cpp', '.c', '.h', '.hpp')):
            path = os.path.join(dirpath, fname)
            with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    m = pattern.match(line)
                    if m:
                        inc = m.group(1)
                        for lib in libs:
                            key = lib.split('/')[-1].split('@')[0].lower()
                            if key in inc.lower():
                                used.add(lib)

print("\nDetected usage:")
for lib in libs:
    state = "used" if lib in used else "(no includes found)"
    print(f"  {lib}: {state}")

unused = [lib for lib in libs if lib not in used]
if unused:
    print("\nConsider removing the following unused dependencies:")
    for lib in unused:
        print("  ", lib)
    # failure exit for CI
    sys.exit(1)
else:
    print("\nAll declared libraries appear referenced in the source.")