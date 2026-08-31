# HP Victus Dynamic Fan Control (`victus-fan-control`)

A lightweight, native C daemon for dynamic hardware fan control on HP Victus & OMEN laptops running Linux.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Language: C11](https://img.shields.io/badge/Language-C11-green.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)]()

> **Language / Dil:** [🇬🇧 English](#table-of-contents) | [🇹🇷 Türkçe](#türkçe-dokümantasyon)

---

## Table of Contents
- [Disclaimer](#disclaimer)
- [Key Features](#key-features)
- [Supported & Tested Devices](#supported--tested-devices)
- [Prerequisites](#prerequisites)
- [Quick Start & Installation](#quick-start--installation)
  - [Compilation](#1-compilation)
  - [Simulation / Test Mode (Dry-Run)](#2-simulation--test-mode-dry-run)
  - [Service Installation (One-Command)](#3-service-installation-one-command)
  - [Clean Uninstallation](#4-clean-uninstallation)
- [Configuration Guide (`config.conf`)](#configuration-guide-configconf)
- [Fan Curve & Linear Interpolation](#fan-curve--linear-interpolation)
- [Safety & Fault-Tolerance Architecture](#safety--fault-tolerance-architecture)
- [Contributing & Reporting Tested Devices](#contributing--reporting-tested-devices)
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

---

## Supported & Tested Devices

- **Tested Hardware:**
  - HP Victus 16-S0010NT (AMD Ryzen 5 7640HS, NVIDIA RTX 4060 Mobile)
- **Supported Hardware:**
  - HP Victus 16 S00xxNT Series (AMD)
- **Potantial Supported Hardware:**
  - HP Victus 15 & 16 Series (Intel/AMD)
  - HP OMEN 15, 16, 17 Series supporting `hp-wmi` fan control
  - Any HP laptop exposing fan controls under `/sys/devices/platform/hp-wmi/hwmon` with `pwm1_enable` and `fan*_target`.

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

### 4. Clean Uninstallation
To completely remove the service and restore full BIOS automatic control:
```bash
sudo make uninstall
```
Once uninstalled, you can safely delete the project directory:
```bash
cd .. && rm -rf victus-fan-control
```

---

## Configuration Guide (`config.conf`)

The configuration file is located at `config.conf` right next to the executable.

| Option | Default | Description |
| :--- | :--- | :--- |
| `check_interval` | `1` | Temperature polling frequency in seconds. |
| `heartbeat_interval` | `10` | Frequency in seconds to refresh sysfs target speeds to keep the EC watchdog alive. |
| `default_speed` | `0` | Fan speed percentage (%) when temperature is below the lowest curve threshold. |
| `temp_critical` | `85` | Critical temperature in °C. Forces 100% emergency fan speed. |
| `fan_curve` | `45:30, 50:40, ...` | Comma-separated list of `temp_celsius:speed_percentage` pairs. |
| `linear_interpolation` | `true` | `true` for smooth linear RPM scaling between points; `false` for discrete step thresholds. |
| `restore_auto_on_exit` | `true` | Restores BIOS automatic fan control when the daemon terminates cleanly. |
| `enable_colors` | `true` | Enables ANSI color output in interactive terminal sessions. |
| `override_fan_dir` | *(empty)* | Custom path to fan hwmon directory (leave empty for auto-detection). |
| `override_cpu_temp_file`| *(empty)* | Custom path to CPU temperature file (leave empty for auto-detection). |

---

## Fan Curve & Linear Interpolation

With `linear_interpolation = true`, the fan speed scales smoothly between points rather than jumping in discrete steps.

**Example Curve:**
```ini
fan_curve = 45:30, 50:40, 60:60, 70:80, 85:95
```

```text
Temperature Curve Mode: Linear Interpolation
  < 45°C       ->   0% (Off / Default)
  45°C - 50°C  ->  30% ~ 40% (Linear slope)
  50°C - 60°C  ->  40% ~ 60% (Linear slope)
  60°C - 70°C  ->  60% ~ 80% (Linear slope)
  70°C - 85°C  ->  80% ~ 95% (Linear slope)
  = 85°C       ->  95%
  > 85°C       -> 100% (Critical Protection Mode)
```

---

## Safety & Fault-Tolerance Architecture

```
                    ┌────────────────────────┐
                    │  Read Sensor (sysfs)   │
                    └───────────┬────────────┘
                                │
                 ┌──────────────┴──────────────┐
                 ▼                             ▼
        [ temp < 0 OR stuck <= 25°C ]    [ Normal Reading ]
                 │                             │
                 ▼                             ▼
         Engage Emergency 100%         Calculate Fan Curve
         Log [ERROR] to stderr          (Linear Interpolation)
                 │                             │
                 └──────────────┬──────────────┘
                                ▼
                    Write Targets to Sysfs
```

1. **Stuck Sensor Watchdog:** If an ACPI glitch causes the sensor to read a stuck value $\le 25^\circ\text{C}$ for 5 consecutive polling cycles, the daemon logs an error to `stderr` and engages 100% emergency cooling.
2. **Emergency Upper Bound:** Any temperature exceeding the highest defined curve point or `temp_critical` immediately triggers 100% fan speed.
3. **Crash Recovery:** Systemd `ExecStopPost` automatically executes `echo 2 > pwm1_enable` upon service exit, crash, or `SIGKILL`.

---

## Contributing & Reporting Tested Devices

Feedback from different HP laptop models is welcome! If you tested this software on your device, please open a GitHub Issue with the following details:

```text
- Laptop Model: HP Victus 16-XXXX / OMEN 16-XXXX
- CPU: (e.g. AMD Ryzen 7 7840HS / Intel Core i7-13700H)
- GPU: (e.g. NVIDIA RTX 4060 / AMD Radeon)
- Linux Kernel: (e.g. uname -r)
- Output of: ./victus-fan-control -t
```

---

<br>

---

# 🇹🇷 Türkçe Dokümantasyon

Linux çalıştıran HP Victus ve OMEN dizüstü bilgisayarlar için saf C11 ile yazılmış, ultra hafif ve dinamik fan kontrol yazılımı.

---

## Sorumluluk Reddi (Disclaimer)

> [!CAUTION]
> **Kullanım tamamen sizin sorumluluğunuzdadır.** Fan hızlarını değiştirmek doğrudan donanım sıcaklığını etkiler. Yanlış veya aşırı agresif düşük hız ayarları, bileşenlerin aşırı ısınmasına (thermal throttling) veya donanım yıpranmasına yol açabilir. Bu yazılım MIT Lisansı altında "olduğu gibi" sağlanmaktadır. Sürekli kullanıma geçmeden önce ayarlarınızı mutlaka `--dry-run` (simülasyon) modunda test ediniz.

---

## Öne Çıkan Özellikler

- **Ultra Hafif (Native C):** Saf C11 ile yazılmıştır. Çalışma zamanı bağımlılığı (Python/Node) yoktur. 2 MB'tan az RAM tüketir.
- **İzole ve Taşınabilir (Self-Contained):** Sistem dizinlerine (`/usr/local/bin`, `/etc`) dosya saçmaz. Kendi klasöründe çalışır.
- **Dinamik Çoklu Fan Tespiti:** Sistemdeki fan sayısını (`fan1`, `fan2` vb.) otomatik algılar ve hepsini dinamik olarak yönetir (1, 2 veya daha fazla fan).
- **Doğrusal Enterpolasyon (Linear Interpolation):** Sıcaklık eşikleri arasında fan devrini pürüzsüzce ölçeklendirir; ani ses ve devir sıçramalarını önler.
- **Gelişmiş Güvenlik Mimarisi:**
  - Sensör hatasında (`temp < 0`) veya 5 ölçüm boyunca $\le 25^\circ\text{C}$ sabit takılı kaldığında otomatik %100 acil durum devrine geçer.
  - Eğri üst sınırı aşıldığında veya kritik sıcaklığa ulaşıldığında fanlar %100 devreye alınır.
  - `ExecStopPost=` mekanizmasıyla program çökse dahi Systemd fan kontrolünü otomatik olarak BIOS'a iade eder.
- **Temiz Loglama:** Servis modunda arka planda sessiz çalışarak SSD log kirliliğini önler; istenirse `-v` ile canlı izleme sunar.

---

## Ön Gereksinimler

- **Linux Çekirdeği:** `hp-wmi` modülü yüklü 6.1+ çekirdek önerilir.
- **Derleme Araçları:** GCC (C11 destekli) ve GNU Make.
- **Yetkiler:** Sysfs fan kontrol dosyalarına yazabilmek için `sudo` (root) yetkisi gereklidir.

Sisteminizde `hp-wmi` kontrolcüsünün aktif olup olmadığını kontrol etmek için:
```bash
ls -d /sys/devices/platform/hp-wmi/hwmon/hwmon*
```

> [!NOTE]
> Eğer mevcut çekirdeğinizde `hp-wmi` fan kontrol dizini görünmüyorsa, fan kontrol desteğini etkinleştirmek için yamalanmış DKMS modülü olan [TUXOV/hp-wmi-fan-and-backlight-control](https://github.com/TUXOV/hp-wmi-fan-and-backlight-control) projesini yükleyebilirsiniz.

---

## Kurulum ve Kullanım

### 1. Derleme
```bash
git clone https://github.com/Umut-Ersoy/victus-fan-control.git
cd victus-fan-control
make
```

### 2. Simülasyon / Test Modu
Donanıma yazmadan güvenle test etmek için:
```bash
# Temel simülasyon (tablo ve ayarları doğrular)
./victus-fan-control -t

# Canlı simülasyon (sıcaklık ve fan devirlerini anlık basar)
./victus-fan-control -t -v
```

### 3. Servis Olarak Kurulum (Tek Komut)
```bash
sudo make install
```
*Bu komut, geçerli proje dizinini dinamik olarak alarak `/etc/systemd/system/victus-fan-control.service` dosyasını oluşturur ve servisi hemen başlatır.*

Servis durumunu ve logları kontrol etmek için:
```bash
systemctl status victus-fan-control.service
journalctl -u victus-fan-control.service -f
```

### 4. Tamamen Kaldırma (Tek Komut)
Servisi durdurmak, silmek ve fan kontrolünü BIOS'a iade etmek için:
```bash
sudo make uninstall
```
Kaldırma tamamlandıktan sonra proje klasörünü güvenle silebilirsiniz:
```bash
cd .. && rm -rf victus-fan-control
```

---

## Konfigürasyon Ayarları (`config.conf`)

Tüm ayarlar çalıştırılabilir dosyanın hemen yanındaki `config.conf` dosyasından yönetilir:

```ini
# Sıcaklık kontrol aralığı (saniye)
check_interval = 1

# EC Watchdog tazeleme aralığı (saniye)
heartbeat_interval = 10

# İlk eşik altındaki varsayılan fan devri (%)
default_speed = 0

# Kritik sıcaklık eşiği (°C - 100% devir zorlar)
temp_critical = 85

# Fan Eğrisi (sıcaklık:yüzde)
fan_curve = 45:30, 50:40, 55:50, 60:60, 65:70, 70:80, 75:85, 80:90, 85:95

# Doğrusal enterpolasyon (true: pürüzsüz lineer geçiş, false: basamaklı)
linear_interpolation = true

# Kapanışta BIOS otomatik moda geri dön
restore_auto_on_exit = true
```

---

## Desteklenen Cihazlar Bildirimi

Yazılımı farklı bir HP modelinde test ettiyseniz, lütfen aşağıdaki şablonla GitHub Issue açarak listeye katkıda bulununuz:

```text
- Cihaz Modeli: HP Victus 16-XXXX / OMEN 16-XXXX
- İşlemci (CPU): (örn. AMD Ryzen 7 7840HS / Intel Core i7-13700H)
- Ekran Kartı (GPU): (örn. NVIDIA RTX 4060 / AMD Radeon)
- Linux Kernel: (örn. uname -r)
- Test Çıktısı: ./victus-fan-control -t
```

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
