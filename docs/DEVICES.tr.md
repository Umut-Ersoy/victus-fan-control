# Desteklenen ve Test Edilen Cihazlar

[🇬🇧 English](DEVICES.md) | [🇹🇷 Türkçe](DEVICES.tr.md)

- **Test Edilen Donanım:**
  - HP Victus 16-S0010NT (AMD Ryzen 5 7640HS, NVIDIA RTX 4060 Mobile)
- **Desteklenen Donanım:**
  - HP Victus 16 S00xxNT Serisi (AMD)
- **Potansiyel Olarak Desteklenen Donanımlar (TEST EDİLMEDİ):**
  - HP Victus 15 ve 16 Serisi (Intel/AMD)
  - `hp-wmi` fan kontrolünü destekleyen HP OMEN 15, 16, 17 Serisi
  - Fan kontrollerini `/sys/devices/platform/hp-wmi/hwmon` altında `pwm1_enable` ve `fan*_target` ile açığa çıkaran herhangi bir HP dizüstü bilgisayar.

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
