#!/bin/bash
#
#   Run clang-tidy over the project's C++ sources and list what it finds.
#
#   Needs the ESP-IDF tools on the path (source ~/.espressif/tools/activate_idf_v6.1.sh):
#   clang-tidy and run-clang-tidy come from the esp-clang package, and the Python used for
#   the helper scripts in tools/clang-tidy is the ESP-IDF one.
#
#   clang-tidy reads the compile commands of a build, so build first: the test build covers
#   components/F7Services and the tests, the main build adds main/.  The database clang-tidy
#   reads is made from those in build/clang-tidy-database, and remade whenever a build has
#   changed them.
#

scriptdir="$( cd "$(dirname "$0")" ; pwd -P )"

CHECKS=
OUTPUT=
SUMMARY=false
FIX=false
REMAKE_DATABASE=false
JOBS=8
FILES=
HELP=false

#
#   Checks used when neither --checks nor a .clang-tidy file says otherwise.  Trailing return
#   types are left out: the codebase does not use them and they swamp everything else.
#
DEFAULT_CHECKS='-*,bugprone-*,modernize-*,performance-*,readability-*,-modernize-use-trailing-return-type'

#
#   Work out which options are on the command line.
#
for i in "$@"
do
case $i in
    -c=*|--checks=*)
    CHECKS="${i#*=}"
    ;;
    -o=*|--output=*)
    OUTPUT="${i#*=}"
    ;;
    -s|--summary)
    SUMMARY=true
    ;;
    --fix)
    FIX=true
    ;;
    -d|--database)
    REMAKE_DATABASE=true
    ;;
    -j=*|--jobs=*)
    JOBS="${i#*=}"
    ;;
    -h|--help)
    HELP=true
    ;;
    -*)
    echo "Unknown command line option ($i)."
    echo "Run clang-tidy.sh --help for information on valid options."
    exit 1
    ;;
    *)
    if [ -n "$FILES" ]; then
        echo "Only one file pattern can be given; use a regular expression such as '(WiFi|System)/'."
        exit 1
    fi
    FILES="$i"
    ;;
esac
done

if $HELP; then
    echo "$0: Run clang-tidy and list the issues it finds."
    echo ""
    echo "Usage: clang-tidy.sh [options] [FILES]"
    echo ""
    echo "FILES is a regular expression matched against the full path of each source file,"
    echo "for example 'Bluetooth/', 'Messaging/Encoders\\.cpp' or '(WiFi|System)/'.  Quote it."
    echo "Without it, every source in components/F7Services, main and test/main is checked."
    echo "Headers are checked through the sources that include them."
    echo ""
    echo "Options:"
    echo "-c=LIST | --checks=LIST  Checks to run, in clang-tidy form: '-*,readability-*,-readability-magic-numbers'"
    echo "                         Default: a .clang-tidy file if there is one, otherwise"
    echo "                         $DEFAULT_CHECKS"
    echo "-o=FILE | --output=FILE  Write the issue list to FILE instead of the terminal"
    echo "-s | --summary           Also print how many issues each check and each file has"
    echo "--fix                    Apply the fixes clang-tidy offers.  Review them, and run the checks"
    echo "                         again: one fix can make another possible"
    echo "-d | --database          Remake the clang-tidy compile database even if it looks current"
    echo "-j=N | --jobs=N          Number of files to check at once (default $JOBS)"
    echo "-h | --help              Display this message"
    echo ""
    echo "Examples:"
    echo "  ./clang-tidy.sh --summary"
    echo "  ./clang-tidy.sh --checks='-*,readability-make-member-function-const' 'Bluetooth/'"
    echo "  ./clang-tidy.sh --checks='-*,modernize-use-auto' --fix 'Messaging/Encoders\\.cpp'"
    echo ""
    exit 0
fi

if ! command -v run-clang-tidy > /dev/null || ! command -v clang-tidy > /dev/null; then
    echo "clang-tidy is not on the path.  Activate ESP-IDF first:"
    echo "  source ~/.espressif/tools/activate_idf_v6.1.sh"
    exit 1
fi

#
#   Make the database clang-tidy reads from whichever build databases exist.  The test one
#   is listed first as it covers the most files.
#
DATABASE_DIR=$scriptdir/build/clang-tidy-database
DATABASE=$DATABASE_DIR/compile_commands.json
MAKE_DATABASE=$scriptdir/tools/clang-tidy/make_database.py

INPUTS=
for candidate in "$scriptdir/test/build/compile_commands.json" "$scriptdir/build/compile_commands.json"
do
    if [ -f "$candidate" ]; then
        INPUTS="$INPUTS $candidate"
        if [ "$candidate" -nt "$DATABASE" ]; then
            REMAKE_DATABASE=true
        fi
    fi
done
if [ -z "$INPUTS" ]; then
    echo "No build to read compile commands from.  Build first, for example:"
    echo "  (cd test && ./build.sh --build)"
    exit 1
fi
if [ ! -f "$DATABASE" ] || [ "$MAKE_DATABASE" -nt "$DATABASE" ]; then
    REMAKE_DATABASE=true
fi
if $REMAKE_DATABASE; then
    python3 "$MAKE_DATABASE" --root "$scriptdir" --output "$DATABASE_DIR" $INPUTS || exit 1
fi

#
#   Run clang-tidy.  The log is kept in a scratch directory and the issues picked out of it.
#
WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

PROJECT_SOURCES="^$scriptdir/(components/F7Services|main|test/main)/"
if [ -z "$FILES" ]; then
    FILES="$PROJECT_SOURCES"
fi

TIDY_OPTIONS=(-p "$DATABASE_DIR" -quiet -j "$JOBS" -header-filter="$PROJECT_SOURCES")
if [ -n "$CHECKS" ]; then
    TIDY_OPTIONS+=(-checks="$CHECKS")
else
    if [ ! -f "$scriptdir/.clang-tidy" ]; then
        TIDY_OPTIONS+=(-checks="$DEFAULT_CHECKS")
    fi
fi
if $FIX; then
    TIDY_OPTIONS+=(-export-fixes "$WORK_DIR/fixes.yaml")
fi

echo "Running clang-tidy on the files matching $FILES"
#
#   run-clang-tidy fails when any file has a compiler error; those are reported below, so
#   its exit status is not needed.
#
run-clang-tidy "${TIDY_OPTIONS[@]}" "$FILES" > "$WORK_DIR/log.txt" 2>&1
grep -m1 '^Running clang-tidy for' "$WORK_DIR/log.txt"

#
#   One line per issue, relative to the project, ordered by file, line and column.
#
grep -E ': (warning|error): ' "$WORK_DIR/log.txt" \
    | sed "s|^$scriptdir/||" \
    | sort -u \
    | sort -t: -k1,1 -k2,2n -k3,3n > "$WORK_DIR/issues.txt"

ISSUE_COUNT=$(grep -c ': warning: ' "$WORK_DIR/issues.txt")
ERROR_COUNT=$(grep -c ': error: ' "$WORK_DIR/issues.txt")

if [ -n "$OUTPUT" ]; then
    cp "$WORK_DIR/issues.txt" "$OUTPUT"
    echo "$ISSUE_COUNT issues written to $OUTPUT"
else
    cat "$WORK_DIR/issues.txt"
    echo "$ISSUE_COUNT issues"
fi

if [ "$ERROR_COUNT" -gt 0 ]; then
    echo ""
    echo "$ERROR_COUNT compiler errors: clang could not parse some files, so their issues are missing."
    echo "If the build is current, try --database to remake the compile database."
fi

if $SUMMARY && [ "$ISSUE_COUNT" -gt 0 ]; then
    echo ""
    echo "Issues by check:"
    grep ': warning: ' "$WORK_DIR/issues.txt" | grep -oE '\[[a-z0-9.,-]+\]$' | sort | uniq -c | sort -rn
    echo ""
    echo "Issues by file:"
    grep ': warning: ' "$WORK_DIR/issues.txt" | cut -d: -f1 | sort | uniq -c | sort -rn
fi

if $FIX; then
    echo ""
    if [ -s "$WORK_DIR/fixes.yaml" ]; then
        python3 "$scriptdir/tools/clang-tidy/apply_fixes.py" "$WORK_DIR/fixes.yaml" || exit 1
        echo "Review the changes, check them with run-checks.sh and run clang-tidy.sh again."
    else
        echo "No fixes to apply."
    fi
fi
