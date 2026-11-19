#!/usr/bin/env python3
"""
Turn the ESP-IDF compile databases into one that clang-tidy can read.

ESP-IDF writes compile_commands.json for the xtensa GCC compiler.  clang-tidy parses each
file with clang, which cannot use those commands as they are:

  - some options are GCC only (-mlongcalls, -fstrict-volatile-bitfields and others);
  - the toolchain's @.../toolchain/cxxflags file selects picolibc with -specs, which clang
    cannot read, so without help clang finds the newlib headers and fails on __FILE;
  - clang has to be told the target.

This keeps the C++ files under the project root, outside managed_components, and rewrites
their commands.  When a file appears in more than one input database, the first one wins.

Usage:
    make_database.py --root PROJECT --output DIRECTORY INPUT [INPUT ...]

Only the Python standard library is needed.
"""

import argparse
import glob
import json
import os
import shlex
import sys

#
#   Options that clang does not understand.  Matched as prefixes, so -Werror covers
#   -Werror=... and @ covers the toolchain response file.
#
GCC_ONLY_OPTIONS = (
    '-fno-tree-switch-conversion',
    '-fstrict-volatile-bitfields',
    '-mno-target-align',
    '-freorder-blocks',
    '-mlongcalls',
    '-fno-shrink-wrap',
    '-fzero-init-padding-bits',
    '-fno-malloc-dce',
    '-specs',
    '-Wno-frame-address',
    '-Werror',
    '@',
)


def ToolchainArguments(compiler):
    """
    Work out the clang arguments that stand in for the GCC toolchain's own settings.

    compiler is the path to xtensa-esp32-elf-gcc (or g++) from the input database.  The
    picolibc headers have to come before the C++ and newlib ones, as they do for GCC.
    """
    root = os.path.dirname(os.path.dirname(compiler))
    libraryVersions = sorted(glob.glob(os.path.join(root, 'xtensa-esp-elf', 'include', 'c++', '*')))
    if not libraryVersions:
        sys.exit(f'make_database.py: no C++ headers found under {root}; is this an xtensa toolchain?')
    cppHeaders = libraryVersions[-1]
    includeDirectories = [
        os.path.join(root, 'picolibc', 'include'),
        cppHeaders,
        os.path.join(cppHeaders, 'xtensa-esp-elf', 'esp32'),
        os.path.join(root, 'xtensa-esp-elf', 'include'),
    ]
    arguments = ['--target=xtensa-esp-elf', '-mcpu=esp32', '-fno-rtti', '-Wno-unknown-warning-option']
    for directory in includeDirectories:
        arguments += ['-isystem', directory]
    return arguments


def IsProjectSource(fileName, root):
    """
    True when fileName is a C++ source of ours: under root and not a managed component.
    """
    path = os.path.realpath(fileName)
    return (path.endswith('.cpp') and path.startswith(root + os.sep) and
            (os.sep + 'managed_components' + os.sep) not in path)


def Main():
    """
    Read the input databases and write the rewritten one.
    """
    parser = argparse.ArgumentParser(description='Make a compile database clang-tidy can read from ESP-IDF ones.')
    parser.add_argument('--root', required=True, help='Project root; only sources under it are kept.')
    parser.add_argument('--output', required=True, help='Directory to write compile_commands.json into.')
    parser.add_argument('inputs', nargs='+', help='ESP-IDF compile_commands.json files.')
    options = parser.parse_args()

    root = os.path.realpath(options.root)
    seen = set()
    result = []
    for inputPath in options.inputs:
        with open(inputPath) as inputFile:
            entries = json.load(inputFile)
        for entry in entries:
            fileName = os.path.realpath(entry['file'])
            if (fileName in seen) or not IsProjectSource(fileName, root):
                continue
            seen.add(fileName)
            arguments = shlex.split(entry['command'])
            kept = [argument for argument in arguments[1:] if not argument.startswith(GCC_ONLY_OPTIONS)]
            entry['command'] = shlex.join(['clang++'] + ToolchainArguments(arguments[0]) + kept)
            result.append(entry)

    os.makedirs(options.output, exist_ok=True)
    outputPath = os.path.join(options.output, 'compile_commands.json')
    with open(outputPath, 'w') as outputFile:
        json.dump(result, outputFile, indent=1)
    print(f'{len(result)} source files in {outputPath}')


if __name__ == '__main__':
    Main()
