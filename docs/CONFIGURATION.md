# Configuration Guide (`config.conf`)

[🇬🇧 English](CONFIGURATION.md) | [🇹🇷 Türkçe](CONFIGURATION.tr.md)

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
