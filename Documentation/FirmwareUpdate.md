# Firmware Updates

This document describes how the ESP32 firmware is updated in the field and why the ESP32 has no over-the-air (OTA) update partitions.

## Who Performs the Update

The update is performed by the managed application, not by the ESP32 firmware.  The ESP32 does not download, receive or write a new image of itself.  The managed application on the STM32 side holds the new firmware files and writes them to the ESP32's flash, replacing the contents of the `factory` application partition.

The ESP32 is a coprocessor with a single peer, the STM32 (see [Communications Overview](CommunicationsOverview.md)).

## No OTA Partitions

OTA updates are not required, so the partition table has no OTA slots and no `otadata` partition.  There is one application partition:

| Name | Purpose |
|---|---|
| `factory` | The application, rewritten in place by an update |

The other partitions are `nvs`, `phy_init`, `coredump` and `littlefs`.  The offsets and sizes for the release and development layouts are in the Flash Partitions tables in `CLAUDE.md`, and the files are `partitions.csv` and `partitions-develop.csv`.

Because the application is rewritten in place there is no fallback image: a failed or interrupted update leaves the device needing to be reflashed.

## Update Files and Checksums

`build.sh` writes an MD5 checksum beside each binary a build produces:

| File | Checksum |
|---|---|
| `build/MeadowComms.bin` | `build/MeadowComms.bin.md5` |
| `build/bootloader/bootloader.bin` | `build/bootloader/bootloader.bin.md5` |
| `build/partition_table/partition-table.bin` | `build/partition_table/partition-table.bin.md5` |

The managed application uses these to check the files it has been given before it flashes them.

## What the Checksum Does and Does Not Establish

An MD5 detects accidental corruption of a file.  It does not detect deliberate substitution, because anyone who can replace the file can replace its checksum.  With the current mechanism the ESP32 side has no way to reject a substituted image: Secure Boot is not enabled and the images are not signed.

Checking a strong digest or a signature before flashing is therefore the responsibility of the managed application, which performs the update and so is the only place the check can be made.  Enabling Secure Boot on the ESP32 is a separate decision, tracked in issue #265.
