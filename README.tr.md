# Victus Fan Control (`victus-fan-control`)

Victus laptoplar için Linux'ta çalışan hafif bir fan kontrol servisi.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Language: C11](https://img.shields.io/badge/Language-C11-green.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-orange.svg)]()

[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

---

## İçindekiler
- [Sorumluluk Reddi](#sorumluluk-reddi)
- [Temel Özellikler](#temel-özellikler)
- [Cihaz Uyumluluğu](#cihaz-uyumluluğu)
- [Ön Koşullar](#ön-koşullar)
- [Hızlı Başlangıç ve Kurulum](#hızlı-başlangıç-ve-kurulum)
  - [Derleme](#1-derleme)
  - [Simülasyon / Test Modu (Dry-Run)](#2-simülasyon--test-modu-dry-run)
  - [Servis Kurulumu (Tek Komut)](#3-servis-kurulumu-tek-komut)
  - [Temiz Kaldırma (Tek Komut)](#4-temiz-kaldırma-tek-komut)
- [Yapılandırma](#yapılandırma)
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

Güvenlik mimarisi ve hata toleransı hakkında daha fazla bilgi için [docs/ARCHITECTURE.tr.md](docs/ARCHITECTURE.tr.md) sayfasına bakın.

---

## Cihaz Uyumluluğu
- **Test Edildi:** HP Victus 16-S0010NT (Ryzen 5 7640HS, RTX 4060)
- **Hedef:** `hp-wmi` aracılığıyla fan sysfs arayüzünü açığa çıkaran HP Victus ve OMEN modelleri.

Test edilen donanımların tam listesi ve kendi cihazınızı bildirme talimatları için [docs/DEVICES.tr.md](docs/DEVICES.tr.md) sayfasına bakın.

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

## Yapılandırma

Tüm ayarlar uygulama dizininde yer alan `config.conf` dosyası üzerinden yönetilir.

Tüm yapılandırma seçenekleri, eğri ayarlama ve doğrusal interpolasyon detayları için [docs/CONFIGURATION.tr.md](docs/CONFIGURATION.tr.md) kılavuzuna bakın.

---

## Lisans

Bu proje MIT Lisansı ile lisanslanmıştır - detaylar için [LICENSE](LICENSE) dosyasına bakın.
