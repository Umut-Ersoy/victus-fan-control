# Yapılandırma Rehberi (`config.conf`)

[🇬🇧 English](CONFIGURATION.md) | [🇹🇷 Türkçe](CONFIGURATION.tr.md)

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
