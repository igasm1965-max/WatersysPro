#!/usr/bin/env python3
"""Strip all code guarded by #if USE_SPIFFS / #endif from source files.

Run this once after deciding to remove SPIFFS support permanently.  The
script walks through C/C++ source files and deletes any region starting
with a preprocessor line containing USE_SPIFFS.  Nested #if/#endif blocks
are handled by a simple counter.

Usage:
    python scripts/remove_spiffs.py

The script edits files in place and saves a `.bak` copy of each modified
file.
"""

import os
import re

root = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

pattern = re.compile(r"^\s*#\s*(if|elif).*USE_SPIFFS")

for dirpath, _, files in os.walk(root):
    for fname in files:
        if fname.endswith(('.cpp', '.h', '.hpp')):
            path = os.path.join(dirpath, fname)
            with open(path, 'r', encoding='utf-8') as f:
                lines = f.readlines()
            out = []
            i = 0
            changed = False
            while i < len(lines):
                line = lines[i]
                if pattern.match(line):
                    # skip until matching endif
                    changed = True
                    depth = 0
                    i += 1
                    while i < len(lines):
                        l = lines[i]
                        if re.match(r"^\s*#\s*(if|ifdef|ifndef)", l):
                            depth += 1
                        elif re.match(r"^\s*#\s*endif", l):
                            if depth == 0:
                                i += 1
                                break
                            depth -= 1
                        i += 1
                    # optionally insert comment
                    out.append("// removed SPIFFS-specific code\n")
                else:
                    out.append(line)
                    i += 1
            if changed:
                bak = path + '.bak'
                os.rename(path, bak)
                with open(path, 'w', encoding='utf-8') as f:
                    f.writelines(out)
                print(f"stripped SPIFFS from {path}, backup saved as {bak}")

