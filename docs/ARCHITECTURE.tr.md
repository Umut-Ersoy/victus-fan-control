# Güvenlik ve Hata Toleransı Mimarisi

[🇬🇧 English](ARCHITECTURE.md) | [🇹🇷 Türkçe](ARCHITECTURE.tr.md)

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
