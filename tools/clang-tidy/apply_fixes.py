#!/usr/bin/env python3
"""
Apply the fixes clang-tidy exported with -export-fixes.

This does the job of clang-apply-replacements, which the ESP-IDF esp-clang package does
not include.  run-clang-tidy checks each source file separately, so a header included by
several of them receives the same fix several times; duplicates are dropped here.  Two
different fixes that overlap are refused, and nothing is written.

Usage:
    apply_fixes.py FIXES.yaml

Needs PyYAML, which the ESP-IDF Python environment provides.
"""

import collections
import sys

import yaml


def ReadReplacements(fixesPath):
    """
    Collect the distinct replacements from the fixes file, grouped by file.
    """
    with open(fixesPath) as fixesFile:
        fixes = yaml.safe_load(fixesFile) or {}
    replacements = collections.defaultdict(set)
    for diagnostic in fixes.get('Diagnostics') or []:
        for replacement in diagnostic['DiagnosticMessage'].get('Replacements') or []:
            replacements[replacement['FilePath']].add(
                (replacement['Offset'], replacement['Length'], replacement['ReplacementText']))
    return replacements


def ApplyToFile(path, replacements):
    """
    Return the contents of path with the replacements made, or exit if two overlap.

    Offsets are in bytes, so the file is handled as bytes and changed from the end back,
    which leaves the offsets of the earlier replacements valid.
    """
    contents = open(path, 'rb').read()
    previousStart = None
    for offset, length, text in sorted(replacements, reverse=True):
        if (previousStart is not None) and (offset + length > previousStart):
            sys.exit(f'apply_fixes.py: overlapping fixes in {path} at byte {offset}; nothing written.')
        contents = contents[:offset] + text.encode() + contents[offset + length:]
        previousStart = offset
    return contents


def Main():
    """
    Apply every fix in the file named on the command line.
    """
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    replacements = ReadReplacements(sys.argv[1])
    #
    #   Work everything out before writing anything, so an overlap leaves no file changed.
    #
    updated = {path: ApplyToFile(path, fileReplacements) for path, fileReplacements in replacements.items()}
    for path, contents in updated.items():
        with open(path, 'wb') as outputFile:
            outputFile.write(contents)
    total = sum(len(fileReplacements) for fileReplacements in replacements.values())
    print(f'{total} replacements made in {len(updated)} files')


if __name__ == '__main__':
    Main()
