#!/usr/bin/env python3
"""
Graph the diagnostic snapshot records in an ESP32 log.

Draws, as PNG files: the free and lowest free internal and DMA heap over time, the load
on each core over time, the stack left in each task over time, and the latest histogram
of each timed metric.  Reads the log the same way as parse_diag.py.

Usage:
    plot_diag.py LOG [LOG ...] [--output DIRECTORY]

Needs matplotlib (pip install matplotlib).
"""

import argparse
import os
import sys

from parse_diag import BUCKETS, last_snapshot, in_snapshot, read_records, to_rows

try:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as pyplot
except ImportError:
    sys.exit("plot_diag.py needs matplotlib: pip install matplotlib")


def seconds(row):
    """Time since boot of a row's snapshot, in seconds (each boot starts again at 0)."""
    return int(row["uptime_ms"] or 0) / 1000


def plot_heap(rows, directory):
    """Free and lowest free heap, internal and DMA, over time."""
    figure, axes = pyplot.subplots(figsize=(10, 5))
    for region in ("internal", "dma"):
        region_rows = [row for row in rows["heap"] if row["region"] == region]
        if not region_rows:
            continue
        times = [seconds(row) for row in region_rows]
        axes.plot(times, [int(row["free"]) for row in region_rows], label=f"{region} free")
        axes.plot(times, [int(row["minimum_free"]) for row in region_rows], linestyle="--", label=f"{region} lowest free")
    axes.set_xlabel("Time since boot (s)")
    axes.set_ylabel("Bytes")
    axes.set_title("Heap")
    axes.legend()
    axes.grid(True, alpha=0.3)
    return save(figure, directory, "heap.png")


def plot_cpu(rows, directory):
    """Load on each core over time."""
    if not rows["cpu"]:
        return None
    figure, axes = pyplot.subplots(figsize=(10, 4))
    times = [seconds(row) for row in rows["cpu"]]
    for core in ("core0_load", "core1_load"):
        values = [row.get(core) for row in rows["cpu"]]
        if all(value not in (None, "") for value in values):
            axes.plot(times, [int(value) for value in values], label=core.replace("_load", ""))
    axes.set_xlabel("Time since boot (s)")
    axes.set_ylabel("Load (%)")
    axes.set_ylim(0, 100)
    axes.set_title("CPU load since the previous snapshot")
    axes.legend()
    axes.grid(True, alpha=0.3)
    return save(figure, directory, "cpu.png")


def plot_stacks(rows, directory):
    """Stack left in each task over time."""
    if not rows["task"]:
        return None
    figure, axes = pyplot.subplots(figsize=(10, 6))
    for name in sorted({row["name"] for row in rows["task"]}):
        task_rows = [row for row in rows["task"] if row["name"] == name]
        axes.plot([seconds(row) for row in task_rows], [int(row["stack_high_water_mark"]) for row in task_rows], label=name)
    axes.set_xlabel("Time since boot (s)")
    axes.set_ylabel("Stack never used (bytes)")
    axes.set_title("Stack high-water mark")
    axes.legend(fontsize="small", ncol=2)
    axes.grid(True, alpha=0.3)
    return save(figure, directory, "stacks.png")


def plot_histograms(rows, directory):
    """The latest histogram of each timed metric."""
    key = last_snapshot(rows)
    histograms = [row for row in in_snapshot(rows["histogram"], key) if int(row["count"] or 0) > 0] if key else []
    if not histograms:
        return None
    figure, axes_list = pyplot.subplots(len(histograms), 1, figsize=(10, 2.5 * len(histograms)), squeeze=False)
    labels = ["0"] + [f"<{1 << bucket}" for bucket in range(1, BUCKETS - 1)] + [f">={1 << (BUCKETS - 2)}"]
    for axes, row in zip((row[0] for row in axes_list), histograms):
        counts = [int(row[f"bucket_{bucket}"] or 0) for bucket in range(BUCKETS)]
        axes.bar(range(BUCKETS), counts)
        axes.set_xticks(range(BUCKETS))
        axes.set_xticklabels(labels, rotation=90, fontsize="x-small")
        axes.set_title(f"{row['name']} ({row['count']} times, us)")
    figure.tight_layout()
    return save(figure, directory, "histograms.png")


def save(figure, directory, name):
    """Save a figure and close it."""
    path = os.path.join(directory, name)
    figure.savefig(path, dpi=120, bbox_inches="tight")
    pyplot.close(figure)
    return path


def main():
    parser = argparse.ArgumentParser(description="Graph the @diag records in an ESP32 log.")
    parser.add_argument("logs", nargs="+", help="Log files holding @diag records, oldest first.")
    parser.add_argument("--output", help="Directory for the PNG files (default: diag-<first log name>).")
    arguments = parser.parse_args()

    rows = to_rows(read_records(arguments.logs))
    if not rows["snapshot"]:
        sys.exit("No @diag records were found.")
    directory = arguments.output or f"diag-{os.path.splitext(os.path.basename(arguments.logs[0]))[0]}"
    os.makedirs(directory, exist_ok=True)
    for path in (plot_heap(rows, directory), plot_cpu(rows, directory), plot_stacks(rows, directory), plot_histograms(rows, directory)):
        if path:
            print(f"Wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
