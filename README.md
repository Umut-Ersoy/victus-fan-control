# Victus Fan Control (`victus-fan-control`)

A lightweight fan control daemon for Victus laptops running Linux.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Language: C11](https://img.shields.io/badge/Language-C11-green.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)]()

[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

---

## Table of Contents
- [Disclaimer](#disclaimer)
- [Key Features](#key-features)
- [Device Compatibility](#device-compatibility)
- [Prerequisites](#prerequisites)
- [Quick Start & Installation](#quick-start--installation)
  - [Compilation](#1-compilation)
  - [Simulation / Test Mode (Dry-Run)](#2-simulation--test-mode-dry-run)
  - [Service Installation (One-Command)](#3-service-installation-one-command)
  - [Clean Uninstallation (One-Command)](#4-clean-uninstallation-one-command)
- [Configuration](#configuration)
- [License](#license)

---

## Disclaimer

> [!CAUTION]
> **Use at your own risk.** Altering cooling fan speeds directly affects thermal behavior. Inappropriate fan curves or turning fans off under heavy load may cause thermal throttling, system instability, or hardware degradation. This software is provided "as is" under the MIT License without warranties of any kind. Always test your configuration in `--dry-run` mode before continuous use.

---

## Key Features

- **Ultra-Lightweight & Fast:** Written in pure C11 with zero runtime dependencies. Uses `< 2 MB` RAM and minimal CPU resources.
- **Self-Contained Architecture:** Operates entirely from its cloned directory. Nothing is scattered across `/usr/local/bin` or `/etc`.
- **Dynamic Fan Auto-Discovery:** Automatically detects and controls 1, 2, or more hardware fans (`fan1`, `fan2`, etc.) via ACPI WMI sysfs.
- **Linear Interpolation:** Smoothly scales fan RPM between custom temperature thresholds, eliminating sudden, noisy RPM jumps.
- **Duplicate Threshold Deduplication:** Automatically sorts fan curve points and resolves identical temperature entries by choosing the safer, higher fan speed.
- **Fault-Tolerant Safety Net:**
  - Emergency 100% fan speed if sensor reading fails (`temp < 0`) or is stuck at `<= 25°C` for 5 consecutive checks.
  - Critical temperature override at `>= temp_critical` or when exceeding the highest curve threshold.
  - Systemd `ExecStopPost=` restores automatic BIOS fan mode even if the process is terminated unexpectedly or crashes.
  - Hardware EC watchdog keep-alive heartbeat.
- **Clean Logging:** Runs completely silent in systemd background mode (preventing SSD journal spam), while offering verbose live monitoring via `-v` / `--verbose`.

For more details on the safety architecture and fault tolerance, see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

---

## Device Compatibility
- **Tested:** HP Victus 16-S0010NT (Ryzen 5 7640HS, RTX 4060)
- **Target:** HP Victus & OMEN models exposing fan sysfs via `hp-wmi`.

For the full list of tested hardware and instructions on reporting your device, see [docs/DEVICES.md](docs/DEVICES.md).

---

## Prerequisites

- **Linux Kernel:** 6.1+ recommended with `hp-wmi` kernel module loaded.
- **Build Tools:** GCC (with C11 support) and GNU Make.
- **Privileges:** Root (`sudo`) is required to write target RPMs to sysfs.

To check if `hp-wmi` is available on your machine:
```bash
ls -d /sys/devices/platform/hp-wmi/hwmon/hwmon*
```

> [!NOTE]
> If the `hp-wmi` sysfs fan control directory is not found on your kernel/BIOS version, you can install the patched DKMS kernel module from [TUXOV/hp-wmi-fan-and-backlight-control](https://github.com/TUXOV/hp-wmi-fan-and-backlight-control) to enable HP WMI fan control support.

---

## Quick Start & Installation

### 1. Compilation
Clone the repository and build the binary:
```bash
git clone https://github.com/Umut-Ersoy/victus-fan-control.git
cd victus-fan-control
make
```

### 2. Simulation / Test Mode (Dry-Run)
Test your configuration without writing to hardware sysfs:
```bash
# Basic simulation (reads config and prints curve table)
./victus-fan-control -t

# Verbose simulation (streams real-time CPU temp and fan RPMs)
./victus-fan-control -t -v
```

### 3. Service Installation (One-Command)
Install and start the systemd background daemon:
```bash
sudo make install
```
*Note: This generates a dynamic `/etc/systemd/system/victus-fan-control.service` pointing directly to your local project directory and immediately starts the service.*

Check service status and logs:
```bash
systemctl status victus-fan-control.service
journalctl -u victus-fan-control.service -f
```

### 4. Clean Uninstallation (One-Command)
To completely remove the service and restore full BIOS automatic control:
```bash
sudo make uninstall
```
Once uninstalled, you can safely delete the project directory:
```bash
cd .. && rm -rf victus-fan-control
```

---

## Configuration

All configuration is managed via the `config.conf` file located in the application directory.

For detailed explanation of all configuration parameters, curve tuning, and linear interpolation, see [docs/CONFIGURATION.md](docs/CONFIGURATION.md).

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.