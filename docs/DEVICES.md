# Supported & Tested Devices

[🇬🇧 English](DEVICES.md) | [🇹🇷 Türkçe](DEVICES.tr.md)

- **Tested Hardware:**
  - HP Victus 16-S0010NT (AMD Ryzen 5 7640HS, NVIDIA RTX 4060 Mobile)
- **Supported Hardware:**
  - HP Victus 16 S00xxNT Series (AMD)
- **Potentially Supported Hardwares (NOT TESTED):**
  - HP Victus 15 & 16 Series (Intel/AMD)
  - HP OMEN 15, 16, 17 Series supporting `hp-wmi` fan control
  - Any HP laptop exposing fan controls under `/sys/devices/platform/hp-wmi/hwmon` with `pwm1_enable` and `fan*_target`.

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
