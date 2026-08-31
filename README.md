# Victus Fan Control (`victus-fan-control`)

A lightweight fan control daemon for Victus laptops running Linux.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Language: C11](https://img.shields.io/badge/Language-C11-green.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)]()

[🇬🇧 English](#table-of-contents) | [🇹🇷 Türkçe](#-t%C3%BCrk%C3%A7e-dok%C3%BCmantasyon)

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
  - [Clean Uninstallation (One-Command)](#4-clean-uninstallation-one-command)
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
- **Potentially Supported Hardwares (NOT TESTED):**
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

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

# 🇹🇷 Türkçe Dokümantasyon

Victus laptoplar için Linux'ta çalışan hafif bir fan kontrol servisi.

## İçindekiler
- [Sorumluluk Reddi](#sorumluluk-reddi)
- [Temel Özellikler](#temel-özellikler)
- [Desteklenen ve Test Edilen Cihazlar](#desteklenen-ve-test-edilen-cihazlar)
- [Ön Koşullar](#ön-koşullar)
- [Hızlı Başlangıç ve Kurulum](#hızlı-başlangıç-ve-kurulum)
  - [Derleme](#1-derleme)
  - [Simülasyon / Test Modu (Dry-Run)](#2-simülasyon--test-modu-dry-run)
  - [Servis Kurulumu (Tek Komut)](#3-servis-kurulumu-tek-komut)
  - [Temiz Kaldırma (Tek Komut)](#4-temiz-kaldırma-tek-komut)
- [Yapılandırma Rehberi (`config.conf`)](#yapılandırma-rehberi-configconf)
- [Fan Eğrisi ve Doğrusal İnterpolasyon](#fan-eğrisi-ve-doğrusal-i̇nterpolasyon)
- [Güvenlik ve Hata Toleransı Mimarisi](#güvenlik-ve-hata-toleransı-mimarisi)
- [Katkıda Bulunma ve Test Edilen Cihazları Bildirme](#katkıda-bulunma-ve-test-edilen-cihazları-bildirme)
- [Lisans](#lisans)

---

## Sorumluluk Reddi

> [!CAUTION]
> **Riski size aittir.** Soğutma fanı hızlarını değiştirmek termal davranışı doğrudan etkiler. Uygun olmayan fan eğrileri veya ağır yük altındayken fanların kapatılması termal kısmaya (thermal throttling), sistem kararsızlığına veya donanım bozulmasına neden olabilir. Bu yazılım, MIT Lisansı altında hiçbir garanti olmaksızın "olduğu gibi" sağlanmaktadır. Sürekli kullanımdan önce yapılandırmanızı her zaman `--dry-run` modunda test edin.

---

## Temel Özellikler

- **Ultra Hafif ve Hızlı:** Sıfır çalışma zamanı bağımlılığı ile saf C11'de yazılmıştır. `< 2 MB` RAM ve minimum CPU kaynağı kullanır.
- **Bağımsız Mimari:** Tamamen klonlandığı dizinden çalışır. `/usr/local/bin` veya `/etc` dizinlerine hiçbir şey dağıtılmaz.
- **Dinamik Otomatik Fan Keşfi:** ACPI WMI sysfs aracılığıyla 1, 2 veya daha fazla donanım fanını (`fan1`, `fan2`, vb.) otomatik olarak algılar ve kontrol eder.
- **Doğrusal İnterpolasyon:** Özel sıcaklık eşikleri arasında fan RPM'ini pürüzsüz bir şekilde ölçeklendirerek ani ve gürültülü RPM sıçramalarını ortadan kaldırır.
- **Yinelenen Eşik Tekilleştirme:** Fan eğrisi noktalarını otomatik olarak sıralar ve daha güvenli, daha yüksek olan fan hızını seçerek aynı sıcaklık girişlerini çözer.
- **Hata Toleranslı Güvenlik Ağı:**
  - Sensör okuması başarısız olursa (`temp < 0`) veya peş peşe 5 kontrol boyunca `<= 25°C`'de takılı kalırsa %100 acil durum fan hızı.
  - `>= temp_critical` olduğunda veya en yüksek eğri eşiği aşıldığında kritik sıcaklık geçersiz kılması (override).
  - İşlem beklenmedik bir şekilde sonlandırılsa veya çökse bile Systemd `ExecStopPost=`, BIOS otomatik fan modunu geri yükler.
  - Donanım EC watchdog ayakta tutma sinyali (heartbeat).
- **Temiz Günlükleme (Logging):** Systemd arka plan modunda (SSD journal spam'ini önleyerek) tamamen sessiz çalışırken, `-v` / `--verbose` aracılığıyla ayrıntılı canlı izleme sunar.

---

## Desteklenen ve Test Edilen Cihazlar

- **Test Edilen Donanım:**
  - HP Victus 16-S0010NT (AMD Ryzen 5 7640HS, NVIDIA RTX 4060 Mobile)
- **Desteklenen Donanım:**
  - HP Victus 16 S00xxNT Serisi (AMD)
- **Potansiyel Olarak Desteklenen Donanımlar (TEST EDİLMEDİ):**
  - HP Victus 15 ve 16 Serisi (Intel/AMD)
  - `hp-wmi` fan kontrolünü destekleyen HP OMEN 15, 16, 17 Serisi
  - Fan kontrollerini `/sys/devices/platform/hp-wmi/hwmon` altında `pwm1_enable` ve `fan*_target` ile açığa çıkaran herhangi bir HP dizüstü bilgisayar.

---

## Ön Koşullar

- **Linux Çekirdeği:** `hp-wmi` çekirdek modülü yüklü olarak 6.1+ önerilir.
- **Derleme Araçları:** GCC (C11 destekli) ve GNU Make.
- **Ayrıcalıklar:** Hedef RPM'leri sysfs'e yazmak için Root (`sudo`) yetkisi gereklidir.

Makinenizde `hp-wmi` bulunup bulunmadığını kontrol etmek için:
```bash
ls -d /sys/devices/platform/hp-wmi/hwmon/hwmon*
```

> [!NOTE]
> Eğer çekirdek/BIOS sürümünüzde `hp-wmi` sysfs fan kontrol dizini bulunamazsa, HP WMI fan kontrol desteğini etkinleştirmek için yamalanmış DKMS çekirdek modülünü [TUXOV/hp-wmi-fan-and-backlight-control](https://github.com/TUXOV/hp-wmi-fan-and-backlight-control) adresinden kurabilirsiniz.

---

## Hızlı Başlangıç ve Kurulum

### 1. Derleme
Depoyu klonlayın ve binary dosyasını derleyin:
```bash
git clone https://github.com/Umut-Ersoy/victus-fan-control.git
cd victus-fan-control
make
```

### 2. Simülasyon / Test Modu (Dry-Run)
Yapılandırmanızı donanım sysfs'ine yazmadan test edin:
```bash
# Temel simülasyon (yapılandırmayı okur ve eğri tablosunu yazdırır)
./victus-fan-control -t

# Ayrıntılı simülasyon (gerçek zamanlı CPU sıcaklığını ve fan RPM'lerini canlı akış olarak gösterir)
./victus-fan-control -t -v
```

### 3. Servis Kurulumu (Tek Komut)
Systemd arka plan programını kurun ve başlatın:
```bash
sudo make install
```
*Not: Bu, doğrudan yerel proje dizininize işaret eden dinamik bir `/etc/systemd/system/victus-fan-control.service` oluşturur ve servisi hemen başlatır.*

Servis durumunu ve günlükleri kontrol edin:
```bash
systemctl status victus-fan-control.service
journalctl -u victus-fan-control.service -f
```

### 4. Temiz Kaldırma (Tek Komut)
Servisi tamamen kaldırmak ve tam BIOS otomatik kontrolünü geri yüklemek için:
```bash
sudo make uninstall
```
Kaldırıldıktan sonra, proje dizinini güvenle silebilirsiniz:
```bash
cd .. && rm -rf victus-fan-control
```

---

## Yapılandırma Rehberi (`config.conf`)

Yapılandırma dosyası, yürütülebilir dosyanın hemen yanındaki `config.conf` konumundadır.

| Seçenek | Varsayılan | Açıklama |
| :--- | :--- | :--- |
| `check_interval` | `1` | Saniye cinsinden sıcaklık sorgulama sıklığı. |
| `heartbeat_interval` | `10` | EC watchdog'u ayakta tutmak için sysfs hedef hızlarını yenileme sıklığı (saniye cinsinden). |
| `default_speed` | `0` | Sıcaklık en düşük eğri eşiğinin altında olduğunda fan hızı yüzdesi (%). |
| `temp_critical` | `85` | °C cinsinden kritik sıcaklık. %100 acil durum fan hızını zorlar. |
| `fan_curve` | `45:30, 50:40, ...` | Virgülle ayrılmış `sicaklik_celsius:hiz_yuzdesi` çiftleri listesi. |
| `linear_interpolation` | `true` | Noktalar arası pürüzsüz doğrusal RPM ölçeklendirmesi için `true`; ayrık adım eşikleri için `false`. |
| `restore_auto_on_exit` | `true` | Arka plan programı temiz bir şekilde sonlandığında BIOS otomatik fan kontrolünü geri yükler. |
| `enable_colors` | `true` | Etkileşimli terminal oturumlarında ANSI renk çıktısını etkinleştirir. |
| `override_fan_dir` | *(boş)* | Fan hwmon dizinine özel yol (otomatik algılama için boş bırakın). |
| `override_cpu_temp_file`| *(boş)* | CPU sıcaklık dosyasına özel yol (otomatik algılama için boş bırakın). |

---

## Fan Eğrisi ve Doğrusal İnterpolasyon

`linear_interpolation = true` ile fan hızı ayrık adımlarla atlamak yerine noktalar arasında pürüzsüz bir şekilde ölçeklenir.

**Örnek Eğri:**
```ini
fan_curve = 45:30, 50:40, 60:60, 70:80, 85:95
```

```text
Sıcaklık Eğrisi Modu: Doğrusal İnterpolasyon
  < 45°C       ->   0% (Kapalı / Varsayılan)
  45°C - 50°C  ->  30% ~ 40% (Doğrusal eğim)
  50°C - 60°C  ->  40% ~ 60% (Doğrusal eğim)
  60°C - 70°C  ->  60% ~ 80% (Doğrusal eğim)
  70°C - 85°C  ->  80% ~ 95% (Doğrusal eğim)
  = 85°C       ->  95%
  > 85°C       -> 100% (Kritik Koruma Modu)
```

---

## Güvenlik ve Hata Toleransı Mimarisi

```
                    ┌───────────────────────────┐
                    │    Sensörü Oku (sysfs)    │
                    └────────────┬──────────────┘
                                 │
                 ┌───────────────┴───────────────┐
                 ▼                               ▼
 [ sıc. < 0 VEYA <= 25°C takılı ]      [ Normal Okuma ]
                 │                               │
                 ▼                               ▼
    Acil Durum %100'ü Devreye Al       Fan Eğrisini Hesapla
     stderr'e [HATA] Günlüğü Yaz      (Doğrusal İnterpolasyon)
                 │                               │
                 └───────────────┬───────────────┘
                                 ▼
                       Hedefleri Sysfs'e Yaz
```

1. **Takılı Sensör Watchdog:** Eğer bir ACPI hatası, sensörün peş peşe 5 sorgulama döngüsü boyunca $\le 25^\circ\text{C}$'de takılı bir değer okumasına neden olursa, arka plan programı `stderr`'e bir hata günlüğü yazar ve %100 acil durum soğutmasını devreye alır.
2. **Acil Durum Üst Sınırı:** Tanımlanan en yüksek eğri noktasını veya `temp_critical` değerini aşan herhangi bir sıcaklık anında %100 fan hızını tetikler.
3. **Çökme Kurtarması:** Systemd `ExecStopPost`, servis çıkışında, çökmesinde veya `SIGKILL` durumunda otomatik olarak `echo 2 > pwm1_enable` komutunu çalıştırır.

---

## Katkıda Bulunma ve Test Edilen Cihazları Bildirme

Farklı HP dizüstü bilgisayar modellerinden gelecek geri bildirimleri memnuniyetle karşılıyoruz! Eğer bu yazılımı cihazınızda test ettiyseniz, lütfen aşağıdaki detaylarla birlikte bir GitHub Issue açın:

```text
- Dizüstü Bilgisayar Modeli: HP Victus 16-XXXX / OMEN 16-XXXX
- İşlemci: (ör. AMD Ryzen 7 7840HS / Intel Core i7-13700H)
- Ekran Kartı: (ör. NVIDIA RTX 4060 / AMD Radeon)
- Linux Çekirdeği: (ör. uname -r)
- Çıktısı: ./victus-fan-control -t
```

---

<br>

---

## Lisans

Bu proje MIT Lisansı ile lisanslanmıştır - detaylar için [LICENSE](LICENSE) dosyasına bakın.