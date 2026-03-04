#!/usr/bin/env python3
"""Identify potentially unused functions and globals in C/C++ source.

Usage: python scripts/find_unused_symbols.py

The script scans all .cpp/.c/.h/.hpp files under the workspace for
function definitions and global variable definitions.  It then searches
for references to each name elsewhere in the code.  If only the definition
itself is found, the symbol is reported as probably unused.

This is a heuristic; false positives are expected (e.g. callbacks termed via
name strings, interrupts, Arduino framework, etc.).  Review the results
before deleting anything.
"""

import os
import re

root = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))

# regex for function definitions (simple, not perfect)
func_def_re = re.compile(r'''^
    \s*(?:static\s+)?               # optional static
    (?:[\w:\<\>\*&\s]+?)         # return type
    \s+([A-Za-z_]\w*)              # function name (group 1)
    \s*\([^;]*\)                  # argument list
    \s*(?:\{)                      # opening brace
    ''', re.VERBOSE)

# regex for global variable/const definitions
var_def_re = re.compile(r'''^
    \s*(?:static\s+)?               # optional static
    (?:const\s+)?                   # optional const
    (?:[\w:\<\>\*&\s]+?)        # type
    \s+([A-Za-z_]\w*)              # var name
    \s*(?:=|;|\[)                  # assignment, semicolon or array
    ''', re.VERBOSE)

symbols = {}  # name -> (kind, file, line)

for dirpath, _, files in os.walk(root):
    for fname in files:
        if not fname.endswith(('.cpp', '.c', '.h', '.hpp')):
            continue
        path = os.path.join(dirpath, fname)
        with open(path, 'r', encoding='utf-8', errors='ignore') as f:
            for lineno, line in enumerate(f, start=1):
                m = func_def_re.match(line)
                if m:
                    name = m.group(1)
                    symbols[name] = ('function', path, lineno)
                else:
                    m2 = var_def_re.match(line)
                    if m2:
                        name = m2.group(1)
                        symbols[name] = ('variable', path, lineno)

# now search for references
unused = []
for name, (kind, path, lineno) in symbols.items():
    occurrences = 0
    for dirpath, _, files in os.walk(root):
        for fname in files:
            if not fname.endswith(('.cpp', '.c', '.h', '.hpp')):
                continue
            p = os.path.join(dirpath, fname)
            with open(p, 'r', encoding='utf-8', errors='ignore') as f:
                for num, line in enumerate(f, start=1):
                    if name in line:
                        # skip the definition line itself
                        if p == path and num == lineno:
                            continue
                        occurrences += 1
                        if occurrences > 0:
                            break
            if occurrences > 0:
                break
    if occurrences == 0:
        unused.append((name, kind, path, lineno))

if unused:
    print("Potentially unused symbols:")
    for name, kind, path, lineno in unused:
        print(f"  {kind} {name} defined at {path}:{lineno}")
else:
    print("No unused symbols detected (by simple heuristic).")
