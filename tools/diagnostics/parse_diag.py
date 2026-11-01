#!/usr/bin/env python3
"""
Turn the diagnostic snapshot records in an ESP32 log into CSV files and a summary.

The firmware writes each snapshot as a set of lines starting @diag (see
Documentation/Diagnostics.md).  This picks those lines out of anything that contains
them, a monitor log, a run-tests.sh transcript or a capture of the UDP log, and writes
one CSV file per kind of record, with the time since boot of the snapshot added to each
row.  It then prints a summary of the last snapshot.

Usage:
    parse_diag.py LOG [LOG ...] [--output DIRECTORY] [--quiet]

Only the Python standard library is needed.
"""

import argparse
import csv
import os
import re
import sys

#
#   Fields of each kind of record after "@diag,<kind>,".  Every record starts with the
#   sequence number of its snapshot.
#
BUCKETS = 24
FIELDS = {
    "snapshot": ["sequence", "uptime_ms", "reset_reason", "free_heap"],
    "task": ["sequence", "number", "name", "core", "state", "priority", "base_priority", "stack_high_water_mark", "run_time_us", "cpu_percent"],
    "cpu": ["sequence", "interval_us", "core0_load", "core1_load"],
    "heap": ["sequence", "region", "total", "free", "minimum_free", "largest_free_block", "free_blocks", "fragmentation"],
    "taskheap": ["sequence", "task", "alive", "current", "peak"],
    "counter": ["sequence", "name", "value"],
    "gauge": ["sequence", "name", "value", "maximum"],
    "histogram": ["sequence", "name", "count", "total_us", "maximum_us"] + [f"bucket_{index}" for index in range(BUCKETS)],
    "network": ["sequence", "protocol", "sent", "received", "dropped", "out_of_memory", "errors"],
    "end": ["sequence", "duration_us"],
}

RECORD = re.compile(r"@diag,([a-z]+),([^\r\n\x1b]*)")


def read_records(paths):
    """Read the @diag records from the files, numbering the boots.

    The sequence number starts again at 1 after a reset, so a snapshot record whose
    sequence number is not higher than the last one starts a new boot.  Returns a list
    of (boot, kind, fields) in the order they were found.
    """
    records = []
    boot = 0
    last_sequence = 0
    for path in paths:
        with open(path, "r", encoding="utf-8", errors="replace") as log:
            for line in log:
                for match in RECORD.finditer(line):
                    kind = match.group(1)
                    values = match.group(2).split(",")
                    if kind not in FIELDS:
                        continue
                    try:
                        sequence = int(values[0])
                    except (ValueError, IndexError):
                        continue
                    if kind == "snapshot":
                        if sequence <= last_sequence:
                            boot += 1
                        last_sequence = sequence
                    records.append((boot, kind, values))
    return records


def to_rows(records):
    """Turn the records into rows: a dictionary per record, by kind.

    Each row gains the boot number and the uptime of its snapshot.
    """
    uptime = {}
    for boot, kind, values in records:
        if kind == "snapshot" and len(values) > 1:
            uptime[(boot, values[0])] = values[1]
    rows = {kind: [] for kind in FIELDS}
    for boot, kind, values in records:
        names = FIELDS[kind]
        row = {"boot": boot, "uptime_ms": uptime.get((boot, values[0]), "")}
        for index, name in enumerate(names):
            if name == "uptime_ms":
                continue
            row[name] = values[index] if index < len(values) else ""
        rows[kind].append(row)
    return rows


def write_csv(rows, directory):
    """Write one CSV file per kind of record that has rows."""
    os.makedirs(directory, exist_ok=True)
    written = []
    for kind, kind_rows in rows.items():
        if not kind_rows:
            continue
        names = ["boot", "uptime_ms"] + [name for name in FIELDS[kind] if name != "uptime_ms"]
        path = os.path.join(directory, f"{kind}.csv")
        with open(path, "w", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=names)
            writer.writeheader()
            writer.writerows(kind_rows)
        written.append(path)
    return written


def bucket_upper_bound(bucket):
    """Longest time, in microseconds, that falls in a histogram bucket (the last is open)."""
    return 0 if bucket == 0 else (1 << bucket) - 1


def percentile(row, fraction):
    """Upper bound of the bucket that holds the given fraction of a histogram's times."""
    count = int(row["count"] or 0)
    if count == 0:
        return None
    target = fraction * count
    running = 0
    for bucket in range(BUCKETS):
        running += int(row[f"bucket_{bucket}"] or 0)
        if running >= target:
            return bucket_upper_bound(bucket) if bucket < (BUCKETS - 1) else int(row["maximum_us"] or 0)
    return int(row["maximum_us"] or 0)


def last_snapshot(rows):
    """Boot and sequence number of the last snapshot that finished."""
    if rows["end"]:
        last = rows["end"][-1]
    elif rows["snapshot"]:
        last = rows["snapshot"][-1]
    else:
        return None
    return last["boot"], last["sequence"]


def in_snapshot(kind_rows, key):
    """Rows of one snapshot."""
    return [row for row in kind_rows if (row["boot"], row["sequence"]) == key]


def print_summary(rows):
    """Print the last snapshot in a form that can be read at a glance."""
    key = last_snapshot(rows)
    if key is None:
        print("No @diag records were found.")
        return
    snapshots = len(rows["snapshot"])
    boots = len({row["boot"] for row in rows["snapshot"]})
    header = in_snapshot(rows["snapshot"], key)
    uptime = int(header[0]["uptime_ms"]) / 1000 if header else 0
    print(f"{snapshots} snapshot(s) over {boots} boot(s).  Last: number {key[1]} at {uptime:.1f} s.")

    for row in in_snapshot(rows["end"], key):
        print(f"  Taking it took {int(row['duration_us']) / 1000:.1f} ms.")
    for row in in_snapshot(rows["cpu"], key):
        print(f"  CPU load: core 0 {row['core0_load']}%, core 1 {row['core1_load']}% over {int(row['interval_us']) / 1000000:.1f} s.")

    tasks = in_snapshot(rows["task"], key)
    if tasks:
        working = [row for row in tasks if not row["name"].startswith("IDLE")]
        busiest = sorted(working, key=lambda row: int(row["cpu_percent"] or 0), reverse=True)[:5]
        print("  Busiest tasks: " + ", ".join(f"{row['name']} {row['cpu_percent']}%" for row in busiest))
        tightest = sorted(tasks, key=lambda row: int(row["stack_high_water_mark"] or 0))[:5]
        print("  Least stack left: " + ", ".join(f"{row['name']} {row['stack_high_water_mark']}" for row in tightest))

    for row in in_snapshot(rows["heap"], key):
        print(f"  Heap {row['region']:8}: {int(row['free']):>7} free of {int(row['total']):>7}, lowest {int(row['minimum_free']):>7}, largest block {int(row['largest_free_block']):>7}, fragmentation {row['fragmentation']}%")

    counters = in_snapshot(rows["counter"], key)
    if counters:
        print("  Counters: " + ", ".join(f"{row['name']} {row['value']}" for row in counters))
    for row in in_snapshot(rows["gauge"], key):
        print(f"  Gauge {row['name']}: {row['value']} (highest {row['maximum']})")
    for row in in_snapshot(rows["histogram"], key):
        count = int(row["count"] or 0)
        if count == 0:
            print(f"  {row['name']}: no times recorded")
            continue
        mean = int(row["total_us"]) / count
        print(f"  {row['name']}: {count} times, mean {mean:.0f} us, 50% under {percentile(row, 0.5)} us, 90% under {percentile(row, 0.9)} us, 99% under {percentile(row, 0.99)} us, longest {row['maximum_us']} us")
    for row in in_snapshot(rows["network"], key):
        print(f"  Network {row['protocol']:4}: sent {row['sent']}, received {row['received']}, dropped {row['dropped']}, out of memory {row['out_of_memory']}, errors {row['errors']}")


def main():
    parser = argparse.ArgumentParser(description="Turn the @diag records in an ESP32 log into CSV files and a summary.")
    parser.add_argument("logs", nargs="+", help="Log files holding @diag records, oldest first.")
    parser.add_argument("--output", help="Directory for the CSV files (default: diag-<first log name>).")
    parser.add_argument("--quiet", action="store_true", help="Write the CSV files without printing the summary.")
    arguments = parser.parse_args()

    rows = to_rows(read_records(arguments.logs))
    directory = arguments.output or f"diag-{os.path.splitext(os.path.basename(arguments.logs[0]))[0]}"
    written = write_csv(rows, directory)
    if not arguments.quiet:
        print_summary(rows)
        print(f"Wrote {len(written)} CSV file(s) to {directory}.")
    return 0 if rows["snapshot"] else 1


if __name__ == "__main__":
    sys.exit(main())
