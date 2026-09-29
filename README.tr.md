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
